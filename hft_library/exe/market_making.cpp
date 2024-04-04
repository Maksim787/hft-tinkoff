#include "config.h"
#include "runner.h"
#include "strategy.h"


class BboMarketMaking : public Strategy {
private:
    int max_skip_qty;
    int place_qty;

public:
    explicit BboMarketMaking(Runner& runner, const ConfigType& config)
            : Strategy(runner),
              max_skip_qty(config["max_skip_qty"].as<int>()),
              place_qty(config["place_qty"].as<int>()) {}

private:
    template <bool IsBid>
    int FindPx(const OneSideMarketOrderBook<IsBid>& ob) {
        int target_px_ind = 0;
        int cum_qty = 0;
        for (int i = 0; i < ob.depth; ++i) {
            cum_qty += ob.qty[i];
            if (cum_qty > max_skip_qty) {
                target_px_ind = i;
                break;
            }
        }
        // ob.cum_qty[target_px_ind - 1] < max_skip_qty
        // ob.cum_qty[target_px_ind] > max_skip_qty
        if (target_px_ind >= 1 && ob.px[target_px_ind] - ob.px[target_px_ind - 1] > 1) {
            return ob.px[target_px_ind - 1] - ob.Sign();
        }
        return ob.px[target_px_ind];
    }

    void PostOrdersForSide(const LimitOrder* order, int target_px, Direction direction) {
        if (order && order->px == target_px) {
            // Order is correct
            return;
        }
        // Cancel wrong order
        if (order && order->px != target_px) {
            try {
                m_runner.CancelOrder(order->order_id);
            } catch (const ServiceReply& reply) {
                m_logger->warn("Could not cancel the order (possible execution)");
            }
        }
        // Post correct order if possible
        try {
            if (direction == Direction::Buy && m_positions.money >= target_px) {
                m_runner.PostOrder(target_px, std::min(place_qty, m_positions.money / target_px), direction);
            } else if (direction == Direction::Sell && m_positions.qty >= 1) {
                m_runner.PostOrder(target_px, std::min(place_qty, m_positions.qty), direction);
            }
        } catch (const ServiceReply& reply) {
            m_logger->warn("Could not post the order (possibly prohibited short): {} qty={}, px={}", direction, place_qty, target_px);
        }
    }

    void PostOrders() {
        // Classify orders
        const LimitOrder* bid_order = nullptr;
        const LimitOrder* ask_order = nullptr;
        for (const auto&[order_id, order]: m_positions.orders) {
            if (order.qty == 0) {
                continue;
            }
            if (order.direction == Direction::Buy) {
                assert(!bid_order);
                bid_order = &order;
            } else {
                assert(!ask_order);
                ask_order = &order;
            }
        }

        // Find target px
        int target_bid_px = FindPx(m_order_book.bid);
        int target_ask_px = FindPx(m_order_book.ask);
//        std::cout << "[Strategy] bid_px = " << target_bid_px << "; " << " ask_px = " << target_ask_px << std::endl;

        // Post orders
        PostOrdersForSide(bid_order, target_bid_px, Direction::Buy);
        PostOrdersForSide(ask_order, target_ask_px, Direction::Sell);
    }

    void OnConnectorsReadiness() override {
        m_logger->info("All connectors are ready");
        m_logger->info("OrderBook:\n{}\nTrades: {}\nPositions:\n{}", m_order_book, m_trades, m_positions);
        // Post initial orders
        PostOrders();
    }

    void OnOrderBookUpdate() override {
        m_logger->trace("OrderBook update");
        PostOrders();
    }

    void OnTradesUpdate() override {
        m_logger->trace("{}", m_trades);
        PostOrders();
    }

    void OnOurTrade(const LimitOrder& order, int executed_qty) override {
        m_logger->info("Execution: qty={} on order={}\nPositions:\n{}", executed_qty, order, m_positions);
        PostOrders();
    }
};

int main() {
    auto config = read_config();

    Runner::StrategyGetter strategy_getter = [](Runner& runner) {
        return std::make_shared<BboMarketMaking>(runner, runner.GetConfig()["strategy"]);
    };
    Runner runner(config, strategy_getter);
    runner.Start();

    std::cout << "Main thread Sleep" << std::endl;
    std::this_thread::sleep_until(std::chrono::time_point<std::chrono::system_clock>::max());

    std::cout << "Exit 0" << std::endl;
    return 0;
}

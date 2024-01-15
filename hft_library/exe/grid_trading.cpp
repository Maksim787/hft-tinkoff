#include <config.h>
#include <runner.h>
#include <strategy.h>


class ExampleStrategy : public Strategy {
private:
    int max_patience_steps; // TODO: implement
    int order_size;
    int spread;

    int curr_bid_px;
    int curr_ask_px;

public:
    explicit ExampleStrategy(Runner& runner, const ConfigType& config)
            : Strategy(runner),
              max_patience_steps(config["max_patience_steps"].as<int>()),
              order_size(config["order_size"].as<int>()),
              spread(config["spread"].as<int>()),
              curr_bid_px(-1),
              curr_ask_px(-1) {}

private:
    template <bool IsBid>
    const OneSideMarketOrderBook<IsBid>& GetOb() const {
        if constexpr (IsBid) {
            return m_order_book.bid;
        } else {
            return m_order_book.ask;
        }
    }

    template <bool IsBid>
    void PostOrdersForSide() {
        // Get best price in the orderbook
        const OneSideMarketOrderBook<IsBid>& ob = GetOb<IsBid>();
        int best_px = ob.px[0];

        // Calculate maximum qty to place on the side
        int max_post_qty = 0;
        if constexpr (IsBid) {
            // Place all money
            max_post_qty = m_positions.money / best_px / order_size;
        } else {
            // Place all current qty
            max_post_qty = m_positions.qty / order_size;
        }

        // Calculate start px for orders
        int start_target_px = -1;
        if (IsBid && curr_bid_px == -1) {
            if (curr_ask_px != -1) {
                // Create spread
                start_target_px = curr_ask_px - spread;
            } else {
                // Start with the best price
                // TODO: add max_patience_steps parameter
                start_target_px = best_px;
            }
        } else if (!IsBid && curr_ask_px == -1) {
            if (curr_bid_px != -1) {
                // Create spread
                start_target_px = curr_bid_px + spread;
            } else {
                // Start with the best price
                start_target_px = best_px;
            }
        } else {
            if (IsBid) {
                start_target_px = curr_bid_px;
            } else {
                start_target_px = curr_ask_px;
            }
        }
        assert(start_target_px != -1);

        // Update curr_bid_px or curr_ask_px
        if constexpr (IsBid) {
            curr_bid_px = start_target_px;
        } else {
            curr_ask_px = start_target_px;
        }

        // Find already placed orders
        std::set<int> orders_px;
        for (const auto&[order_id, order]: m_positions.orders) {
            if (order.qty == 0) {
                continue;
            }
            if (order.direction == Direction::Buy && IsBid || order.direction == Direction::Sell && !IsBid) {
                if ((IsBid && (order.px <= start_target_px - max_post_qty || order.px > start_target_px)) ||
                    (!IsBid && (order.px >= start_target_px + max_post_qty || order.px < start_target_px))) {
                    m_logger->info("Cancel order at px={}; IsBid={}", order.px, IsBid);
                    m_runner.CancelOrder(order.order_id);
                } else {
                    orders_px.insert(order.px);
                }
            }
        }

        // Calculate bounds for orders
        int finish_target_px = start_target_px - max_post_qty * ob.Sign();

        Direction direction = IsBid ? Direction::Buy : Direction::Sell;
        for (int px = start_target_px; px * ob.Sign() > finish_target_px * ob.Sign(); px -= ob.Sign()) {
            if (!orders_px.contains(px)) {
                m_logger->info("Post order at px={}; IsBid={}", px, IsBid);
                m_runner.PostOrder(px, order_size, direction);
            }
        }
    }

    void PostOrders() {
        PostOrdersForSide<true>();
        PostOrdersForSide<false>();
    }

    void OnConnectorsReadiness() override {
        m_logger->info("All connectors are ready");
        m_logger->info("OrderBook:\n{}\nTrades:{}\nPositions:\n{}", m_order_book, m_trades, m_positions);
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
        // Correct curr_bid_px and curr-ask_px
        if (order.direction == Direction::Buy) {
            curr_bid_px = order.px - 1;
        } else {
            curr_ask_px = order.px + 1;
        }
        PostOrders();
    }
};

int main() {
    auto config = read_config();

    Runner::StrategyGetter strategy_getter = [](Runner& runner) {
        return std::make_shared<ExampleStrategy>(runner, runner.GetConfig()["strategy"]);
    };
    Runner runner(config, strategy_getter);
    runner.Start();

    std::cout << "Main thread Sleep" << std::endl;
    std::this_thread::sleep_until(std::chrono::time_point<std::chrono::system_clock>::max());

    std::cout << "Exit 0" << std::endl;
    return 0;
}

#include <config.h>
#include <runner.h>
#include <strategy.h>


class ExampleStrategy : public Strategy {
private:
    int max_levels;
    int order_size;
    int spread;

    int curr_bid_px;

public:
    explicit ExampleStrategy(Runner& runner, const ConfigType& config)
            : Strategy(runner),
              max_levels(config["max_levels"].as<int>()),
              order_size(config["order_size"].as<int>()),
              spread(config["spread"].as<int>()),
              curr_bid_px(-1) {}

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
        int sign = GetOb<IsBid>().Sign();

        // Update curr_bid_px if necessary
        if (m_positions.money / curr_bid_px / order_size == 0) {
            // We only have the asset -> decrease the curr_bid_px
            curr_bid_px = std::min(curr_bid_px, m_order_book.bid.px[0] - spread + 1);
        }
        if (m_positions.qty / order_size == 0) {
            // We only have money -> increase the curr_bid_px
            curr_bid_px = std::max(curr_bid_px, m_order_book.bid.px[0] - spread + 1);
        }

        // Calculate start px for orders
        int start_target_px = -1;
        if constexpr (IsBid) {
            start_target_px = curr_bid_px;
        } else {
            start_target_px = curr_bid_px + spread;
        }
        assert(start_target_px != -1);

        // Calculate maximum qty to place on the side
        int max_post_qty = 0;
        if constexpr (IsBid) {
            // Place all money
            max_post_qty = std::min(m_positions.money / start_target_px / order_size, max_levels);
        } else {
            // Place all current qty
            max_post_qty = std::min(m_positions.qty / order_size, max_levels);
        }
        // Calculate bounds for orders
        int finish_target_px = start_target_px - max_post_qty * sign;
        // For debug
//        m_logger->trace("IsBid={}, start_target_px={}, finish_target_px={}, max_post_qty={}",
//                        IsBid, start_target_px, finish_target_px, max_post_qty);

        // Find already placed orders
        std::set<int> orders_px;
        std::vector<std::string> cancel_order_ids;
        for (const auto&[order_id, order]: m_positions.orders) {
            if (order.qty == 0) {
                continue;
            }
            if (order.direction == Direction::Buy && IsBid || order.direction == Direction::Sell && !IsBid) {
                if (order.px * sign <= finish_target_px * sign || order.px * sign > start_target_px * sign) {
                    cancel_order_ids.push_back(order_id);
                } else {
                    orders_px.insert(order.px);
                }
            }
        }
        // Cancel inappropriate orders
        for (const std::string& order_id: cancel_order_ids) {
            try {
                const LimitOrder& order = m_positions.orders.at(order_id);
                m_runner.CancelOrder(order_id);
            } catch (const ServiceReply& reply) {
                m_logger->warn("Could not cancel the order (possible execution)");
            }
        }

        Direction direction = IsBid ? Direction::Buy : Direction::Sell;
        for (int px = finish_target_px + sign; px * sign <= start_target_px * sign; px += sign) {
            if (!orders_px.contains(px)) {
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
        curr_bid_px = m_order_book.bid.px[0];
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
        if (order.qty == 0) {
            // Update curr_bid_px only on full execution
            if (order.direction == Direction::Buy) {
                --curr_bid_px;
            } else {
                ++curr_bid_px;
            }
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

#include <config.h>
#include <runner.h>
#include <strategy.h>


class ExampleStrategy : public Strategy {
private:
    int max_levels;
    int order_size;
    int spread;

    std::atomic_int curr_bid_px;

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
        if (m_runner.GetPendingEvents() >= 1) {
            m_logger->warn("Break PostOrdersForSide: {} events pending", m_runner.GetPendingEvents());
            return;
        }
        // Get best price in the orderbook
        int sign = GetOb<IsBid>().Sign();

        // Update curr_bid_px if necessary
        if (m_positions.money / curr_bid_px / order_size == 0) {
            // We only have the asset -> decrease the curr_bid_px
            // ask = best_ask + spread
            curr_bid_px = std::min(static_cast<int>(curr_bid_px), m_order_book.ask.px[0]);
        }
        if (m_positions.qty / order_size == 0) {
            // We only have money -> increase the curr_bid_px
            // bid = best_bid - spread
            curr_bid_px = std::max(static_cast<int>(curr_bid_px), m_order_book.bid.px[0] - spread);
        }

        // Calculate start px for orders
        int start_target_px;
        if constexpr (IsBid) {
            start_target_px = curr_bid_px;
        } else {
            start_target_px = curr_bid_px + spread;
        }

        // Calculate maximum qty to place on the side
        int max_post_qty;
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
            assert(order.qty != 0);
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
            if (m_runner.GetPendingEvents() >= 1) {
                m_logger->warn("Break cancelling orders: {} events pending", m_runner.GetPendingEvents());
                return; // Skip cancelling orders while some events are pending
            }
        }

        Direction direction = IsBid ? Direction::Buy : Direction::Sell;
        for (int px = finish_target_px + sign; px * sign <= start_target_px * sign; px += sign) {
            if (!orders_px.contains(px)) {
                try {
                    m_runner.PostOrder(px, order_size, direction);
                } catch (const ServiceReply& reply) {
                    m_logger->warn("Could not post the order (TODO: fix the cancel order)");
                }
                if (m_runner.GetPendingEvents() >= 1) {
                    m_logger->warn("Break posing orders: {} events pending", m_runner.GetPendingEvents());
                    return; // Skip placing orders while some events are pending
                }
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
        m_logger->trace("OrderBook update.\tcurr_bid_px={}; curr_ask_px={}", curr_bid_px * m_instrument.px_step, (curr_bid_px + spread) * m_instrument.px_step);
        PostOrders();
    }

    void OnTradesUpdate() override {
        m_logger->trace("{}.\tcurr_bid_px={}; curr_ask_px={}", m_trades, curr_bid_px * m_instrument.px_step, (curr_bid_px + spread) * m_instrument.px_step);
        PostOrders();
    }

    void OnOurTradeAsync(const LimitOrder& order, int executed_qty) override {
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
        // Do not post orders, it will be done in OnOurTrade
    }

    void OnOurTrade(const LimitOrder& order, int executed_qty) override {
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

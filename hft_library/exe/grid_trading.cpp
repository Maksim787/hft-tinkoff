#include <deque>
#include <filesystem>

#include "config.h"
#include "runner.h"
#include "strategy.h"

class GridTrading : public Strategy {
   private:
    // Parameters
    const int max_levels;
    const int order_size;
    const int spread;
    const bool debug;

    int m_first_bid_px;   // first_ask = first_bid_px + spread + (first_bid_qty == order_size)
    int m_first_bid_qty;  // first_ask_qty = order_size - target_bid_qty + order_size * (first_bid_qty == order_size)

    std::shared_ptr<spdlog::logger> m_first_quotes_logger;

   public:
    explicit GridTrading(Runner& runner, const ConfigType& config)
        : Strategy(runner),
          max_levels(config["max_levels"].as<int>()),
          order_size(config["order_size"].as<int>()),
          spread(config["spread"].as<int>()),
          debug(config["debug"].as<bool>()),
          m_first_quotes_logger(m_runner.GetLogger("target_bid_ask", true)) {
        assert(spread >= 2);
        // log strategy parameters
        m_logger->info("spread = {}; order_size = {};  max_levels = {}; debug = {}", spread, order_size, max_levels, debug);
        // log first quotes header
        m_first_quotes_logger->info("when,strategy_time,msg,bid_count;bid_px;bid_qty,ask_count;ask_px;ask_qty");
    }

   private:
    // Utils
    template <bool IsBid>
    static constexpr int Sign() {
        if constexpr (IsBid) {
            return 1;
        } else {
            return -1;
        }
    }

    template <bool IsBid>
    const OneSideMarketOrderBook<IsBid>& GetOb() const {
        if constexpr (IsBid) {
            return m_order_book.bid;
        } else {
            return m_order_book.ask;
        }
    }

    bool CheckEventsPending(const std::string& msg) {
        if (m_runner.GetPendingEvents() >= 1) {
            m_logger->info("Break {}: {} events pending", msg, m_runner.GetPendingEvents());
            return true;
        }
        return false;
    }

    template <bool IsBid>
    int GetFirstPx() const {
        assert(0 <= m_first_bid_qty);
        assert(m_first_bid_qty <= order_size);
        if constexpr (IsBid) {
            return m_first_bid_px;
        } else {
            return m_first_bid_px + spread + (m_first_bid_qty == order_size);
        }
    }

    template <bool IsBid>
    int GetFirstQty() const {
        assert(0 <= m_first_bid_qty);
        assert(m_first_bid_qty <= order_size);
        if constexpr (IsBid) {
            assert(m_first_bid_qty <= GetMaxPostQty<true>());
            return m_first_bid_qty;
        } else {
            int first_ask_qty = order_size - m_first_bid_qty + order_size * (m_first_bid_qty == order_size);
            return std::min(first_ask_qty, GetMaxPostQty<IsBid>());
        }
    }

    template <bool IsBid>
    int GetMaxPostQty() const {
        if constexpr (IsBid) {
            return m_positions.money / (m_order_book.bid.px[0] + 5);
        } else {
            return m_positions.qty;
        }
    }

    // Quotes Updates
    void InitializeFirstQuotes() {
        // Calculate best_px
        m_first_bid_px = (m_order_book.bid.px[0] + m_order_book.ask.px[0]) / 2 - spread / 2;

        // Calculate first qty
        m_first_bid_qty = std::min(GetMaxPostQty<true>(), order_size);

        // Log initial quotes
        m_logger->info("InitializeFirstQuotes: first_bid_px={}, fitst_bid_qty={}", m_first_bid_px, m_first_bid_qty);
    }

    void UpdateFirstQuotesOnPriceChange() {
        int first_ask_qty = GetFirstQty<false>();
        assert(m_first_bid_qty > 0 || first_ask_qty > 0);
        int first_bid_px_old = m_first_bid_px;
        if (m_first_bid_qty == 0) {
            // No quotes on bid side
            // We only have the asset -> decrease the m_first_bid_px
            // ask = best_ask + spread
            m_first_bid_px = std::min(m_first_bid_px, m_order_book.ask.px[0]);
        } else if (first_ask_qty == 0) {
            // No quotes on ask side
            // We only have money -> increase the m_first_bid_px
            // bid = best_bid - spread
            m_first_bid_px = std::max(m_first_bid_px, m_order_book.bid.px[0] - spread);
        } else {
            // We have quotes on both sides
        }
        if (first_bid_px_old != m_first_bid_px) {
            m_logger->info("first_bid_px: {} -> {}", first_bid_px_old, m_first_bid_px);
        }
    }

    template <bool IsBid>
    void UpdateFirstQuotesOnExecution(int executed_px, int executed_qty) {
        // IsBid = true -> executed_qty from bids
        int first_bid_px_old = m_first_bid_px;
        int first_bid_qty_old = m_first_bid_qty;

        if constexpr (IsBid) {
            // Update best bid qty
            m_first_bid_qty -= executed_qty;
            if (m_first_bid_qty < 0) {
                // Update best bid price if the whole level on bids was executed
                m_first_bid_qty += order_size;
                --m_first_bid_px;
            }
        } else {
            // Update best bid qty
            m_first_bid_qty += executed_qty;
            if (m_first_bid_qty > order_size) {
                // Update best bid price if the whole level on asks was executed
                m_first_bid_qty -= order_size;
                ++m_first_bid_px;
            }
        }
        m_logger->info("UpdateFirstQuotesOnExecution({}; executed_px={}; executed_qty={}): first_bid_px: {} -> {}; first_bid_qty: {} -> {}", (IsBid ? "bid" : "ask"), executed_px, executed_qty, first_bid_px_old, m_first_bid_px, first_bid_qty_old, m_first_bid_qty);
        assert(m_first_bid_qty >= 0);
        assert(m_first_bid_qty <= order_size);
    }

    template <bool IsBid, bool OnlyCancel>
    bool PostLevels() {
        // Calculate target quotes
        std::map<int, int> new_qty_by_px;
        int max_post_qty = GetMaxPostQty<IsBid>();
        int sign = Sign<IsBid>();
        int first_qty = GetFirstQty<IsBid>();
        int first_px = GetFirstPx<IsBid>();
        if (first_qty > 0) {
            // First quote
            new_qty_by_px[first_px] = first_qty;
            assert(first_qty <= max_post_qty);
            max_post_qty -= first_qty;
            // Other quotes
            for (int i = 1; (i < max_levels) && (max_post_qty > 0); ++i) {
                int new_qty = std::min(order_size, max_post_qty);
                new_qty_by_px[first_px - sign * i] = new_qty;
                max_post_qty -= new_qty;
            }
        }

        // Calculate old quotes
        std::map<int, int> old_qty_by_px;
        for (const auto& [order_id, order] : m_positions.orders) {
            assert(order.qty > 0);
            old_qty_by_px[order.px] += order.qty;
        }
        assert(new_qty_by_px.size() <= max_levels);

        // Find orders to cancel
        std::vector<std::string> cancel_order_ids;
        for (const auto& [order_id, order] : m_positions.orders) {
            assert(order.qty != 0);
            if (order.direction == Direction::Buy && IsBid || order.direction == Direction::Sell && !IsBid) {
                if (!new_qty_by_px.contains(order.px) || new_qty_by_px[order.px] < old_qty_by_px[order.px]) {
                    cancel_order_ids.push_back(order_id);
                    old_qty_by_px[order.px] -= order.qty;
                }
            }
        }
        std::sort(cancel_order_ids.begin(), cancel_order_ids.end(),
                  [this](const std::string& id1, const std::string& id2) {
                      const LimitOrder& order1 = m_positions.orders.at(id1);
                      const LimitOrder& order2 = m_positions.orders.at(id2);
                      if constexpr (IsBid) {
                          return order1.px > order2.px;
                      } else {
                          return order1.px < order2.px;
                      }
                  });

        // Cancel inappropriate orders
        for (const std::string& order_id : cancel_order_ids) {
            try {
                const LimitOrder& order = m_positions.orders.at(order_id);
                m_logger->info("CancelOrder(order_id={}); order={}", order_id, order);
                if (!debug) {
                    m_runner.CancelOrder(order_id);
                }
            } catch (const ServiceReply& reply) {
                m_logger->warn("Could not cancel the order (possible execution). Break posting orders");
                return true;  // Signal to stop the iteration
            }
            if (CheckEventsPending("Cancel Orders")) {
                return true;  // Signal to stop the iteration
            }
        }

        if constexpr (OnlyCancel) {
            return false;  // Continue iteration to cancel orders from other side
        }

        // Place new orders
        auto postOrder = [this, &old_qty_by_px](const auto& pair) {
            Direction direction = IsBid ? Direction::Buy : Direction::Sell;
            const auto [px, qty] = pair;
            const int place_qty = qty - old_qty_by_px[px];
            assert(place_qty <= order_size);
            if (place_qty > 0) {
                try {
                    m_logger->info("PostOrder(px={}, qty={}, direction={})", px, place_qty, direction);
                    if (!debug) {
                        m_runner.PostOrder(px, place_qty, direction);
                    }
                } catch (const ServiceReply& reply) {
                    m_logger->error("Could not post the order. Break posting orders");
                    return true;  // Signal to stop the iteration
                }
                if (CheckEventsPending("Post Orders")) {
                    return true;  // Signal to stop the iteration
                }
            }
            return false;  // Continue iteration
        };

        if constexpr (IsBid) {
            for (const auto& pair : new_qty_by_px) {
                if (postOrder(pair)) return true;  // Signal to stop the iteration
            }
        } else {
            for (auto it = new_qty_by_px.rbegin(); it != new_qty_by_px.rend(); ++it) {
                if (postOrder(*it)) return true;  // Signal to stop the iteration
            }
        }

        return false;  // Continue iteration
    }

    void PostOrders() {
        if (CheckEventsPending("PostOrders() start")) {
            return;
        }

        // Update quotes on huge price change if necessary
        UpdateFirstQuotesOnPriceChange();

        // Cancel orders
        if (PostLevels<true, true>()) {
            return;
        }
        if (PostLevels<false, true>()) {
            return;
        }

        // Post orders
        if (PostLevels<true, false>()) {
            return;
        }
        if (PostLevels<false, false>()) {
            return;
        }
    }

    void OnConnectorsReadiness() override {
        m_logger->info("All connectors are ready");
        m_logger->info("OrderBook:\n{}\nTrades: {}\nPositions:\n{}", m_order_book, m_trades, m_positions);
        // Initialize first quotes for bid/ask
        InitializeFirstQuotes();
        // Post initial orders
        PostOrders();
    }

    void OnOrderBookUpdate() override {
        // Log event
        m_logger->trace("OrderBook update.\tbid_px={}; ask_px={}; first_bid_px={}; first_bid_qty={}", m_order_book.bid.px[0], m_order_book.ask.px[0], m_first_bid_px, m_first_bid_qty);

        // Handle event
        PostOrders();
    }

    void OnTradesUpdate() override {
        // Log event
        m_logger->trace("OnTradesUpdate update.\tbid_px={}; ask_px={}; first_bid_px={}; first_bid_qty={}. Trade: {}", m_order_book.bid.px[0], m_order_book.ask.px[0], m_first_bid_px, m_first_bid_qty, m_trades);

        // Handle event
        PostOrders();
    }

    void OnOurTrade(const LimitOrder& order, int executed_qty) override {
        // Log event
        m_logger->info("Execution: executed_qty={} on order={}", executed_qty, order);
        m_logger->info("money={}; qty={}; n_orders={}", m_positions.money, m_positions.qty, m_positions.orders.size());

        // Update quotes
        if (order.direction == Direction::Buy) {
            UpdateFirstQuotesOnExecution<true>(order.px, executed_qty);
        } else {
            UpdateFirstQuotesOnExecution<false>(order.px, executed_qty);
        }

        // Handdle event
        PostOrders();
    }
};

int main() {
    auto config = read_config();
    std::filesystem::create_directory(config["runner"]["log_directory"].as<std::string>());

    Runner::StrategyGetter strategy_getter = [](Runner& runner) {
        return std::make_shared<GridTrading>(runner, runner.GetConfig()["strategy"]);
    };
    Runner runner(config, strategy_getter);
    runner.Start();

    std::cout << "Main thread Sleep" << std::endl;
    std::this_thread::sleep_until(std::chrono::time_point<std::chrono::system_clock>::max());

    std::cout << "Exit 0" << std::endl;
    return 0;
}

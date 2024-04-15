#include <deque>
#include <filesystem>

#include "config.h"
#include "runner.h"
#include "strategy.h"

struct TargetQuotesValidator;

class GridTrading : public Strategy {
   private:
    int max_levels;
    int order_size;
    int spread;
    bool debug;

    struct Level {
        int px;
        int qty;

        Level(int px, int qty) : px(px), qty(qty) {}
    };

    std::shared_ptr<spdlog::logger> m_target_bid_ask_logger;
    std::deque<Level> target_bids;
    std::deque<Level> target_asks;
    int bids_asks_qty_sum = 0;

    friend std::ostream& operator<<(std::ostream& out, const std::deque<Level>& target_quotes) {
        // target_quote.size(),px_1,qty_1,...,px_n,qty_n
        out << target_quotes.size();
        for (const Level& level : target_quotes) {
            out << ";" << level.px << ";" << level.qty;
        }
        return out;
    }

   public:
    explicit GridTrading(Runner& runner, const ConfigType& config)
        : Strategy(runner),
          max_levels(config["max_levels"].as<int>()),
          order_size(config["order_size"].as<int>()),
          spread(config["spread"].as<int>()),
          debug(config["debug"].as<bool>()),
          m_target_bid_ask_logger(m_runner.GetLogger("target_bid_ask", true)) {
        assert(spread >= 2);
        m_logger->info("spread = {}; order_size = {};  max_levels = {}; debug = {}", spread, order_size, max_levels, debug);
        m_target_bid_ask_logger->info("when,strategy_time,msg,bid_count;bid_px;bid_qty,ask_count;ask_px;ask_qty");
    }
    friend struct TargetQuotesValidator;

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

    void ValidateTargetQuotes() {
        // One of the arrays is not empty
        assert(!target_bids.empty() || !target_asks.empty());

        // 0 <= qty <= order_size
        int curr_bids_asks_qty_sum = 0;
        for (const Level& level : target_bids) {
            assert(level.qty >= 0 && level.qty <= order_size);
            curr_bids_asks_qty_sum += level.qty;
        }

        // 0 <= qty <= order_size
        int curr_target_asks_qty_sum = 0;
        for (const Level& level : target_asks) {
            assert(level.qty >= 0 && level.qty <= order_size);
            curr_bids_asks_qty_sum += level.qty;
        }
        // sum is constant
        assert(curr_bids_asks_qty_sum == bids_asks_qty_sum);

        // Price step is 1
        for (size_t i = 1; i < target_bids.size(); ++i) {
            assert(target_bids[i].px == target_bids[i - 1].px - 1);
        }
        for (size_t i = 1; i < target_asks.size(); ++i) {
            assert(target_asks[i].px == target_asks[i - 1].px + 1);
        }

        // spread is not more than (spread - 1)
        if (!target_bids.empty() && !target_asks.empty()) {
            assert(target_bids.front().px + spread - 1 <= target_asks.front().px);
        }
    }

    // Quotes Updates
    template <bool IsBid>
    void InitializeTargetQuotes(bool final_validate) {
        const std::string msg = fmt::to_string(fmt::format("InitializeTargetQuotes({})", (IsBid ? "bid" : "ask")));
        TargetQuotesValidator v(*this, msg, false, final_validate);
        constexpr int sign = Sign<IsBid>();
        int start_px = m_order_book.bid.px[0] - (spread - 1) / 2;
        if constexpr (!IsBid) {
            start_px += spread;
        }
        std::deque<Level>& bids = IsBid ? target_bids : target_asks;
        // TODO: dynamically determine maximum qty
        int max_post_qty = IsBid ? m_positions.money / m_order_book.bid.px[0] : m_positions.qty;
        m_logger->info("{}: max_post_qty={}", msg, max_post_qty);
        int curr_qty = 0;
        for (int i = 0; i < max_post_qty; ++i) {
            int new_qty = std::min(max_post_qty - curr_qty, order_size);
            if (new_qty == 0) {
                break;
            }
            bids.emplace_back(start_px - sign * i, new_qty);
            curr_qty += new_qty;
        }
        bids_asks_qty_sum += max_post_qty;
    }

    template <bool IsBid>
    void UpdateTargetQuotesOnPriceChange() {
        const std::string msg = fmt::to_string(fmt::format("UpdateTargetQuotesOnPriceChange({})", (IsBid ? "bid" : "ask")));
        TargetQuotesValidator v(*this, std::move(msg));
        // We only have money -> increase the curr_bid_px
        // bid = best_bid - spread
        // We only have the asset -> decrease the curr_bid_px
        // ask = best_ask + spread
        constexpr int sign = Sign<IsBid>();
        std::deque<Level>& bids = IsBid ? target_bids : target_asks;
        assert(!bids.empty());
        int target_bid_px = IsBid ? m_order_book.bid.px[0] - spread : m_order_book.ask.px[0] + spread;
        int front_bid_px = bids.front().px;
        if (front_bid_px * sign < target_bid_px * sign) {
            for (Level& level : bids) {
                level.px += (target_bid_px - front_bid_px);
            }
        }
    }

    template <bool IsBid>
    void UpdateTargetQuotesOnExecution(int executed_px, int executed_qty) {
        // IsBid = true -> executed_qty from bids
        const std::string msg = fmt::to_string(fmt::format("UpdateTargetQuotesOnExecution({}; px={}; qty={})", (IsBid ? "bid" : "ask"), executed_px, executed_qty));
        TargetQuotesValidator v(*this, msg);
        std::deque<Level>& bids = IsBid ? target_bids : target_asks;
        std::deque<Level>& asks = IsBid ? target_asks : target_bids;
        constexpr int sign = Sign<IsBid>();
        while (executed_qty > 0) {
            assert(!bids.empty());
            // Get executed bid
            Level& bid_front = bids.front();
            int bid_front_px = bid_front.px;
            // Remove executed qty from this bid
            int remove_qty = std::min(executed_qty, bid_front.qty);
            bid_front.qty -= remove_qty;
            executed_qty -= remove_qty;
            // Remove empty bid
            if (bid_front.qty == 0) {
                bids.pop_front();
            }
            if (asks.empty() || asks.front().qty == order_size) {
                // Add new ask
                asks.emplace_front(bid_front_px + sign * (spread - 1), remove_qty);
            } else {
                // Increase the current ask
                Level& ask_front = asks.front();
                int delta_qty = std::min(remove_qty, order_size - ask_front.qty);
                ask_front.qty += delta_qty;
                remove_qty -= delta_qty;
                // Add new ask if needed
                if (remove_qty != 0) {
                    asks.emplace_front(ask_front.px - sign, remove_qty);
                }
            }
        }
    }

    template <bool IsBid, bool OnlyCancel>
    bool PostLevels() {
        std::deque<Level>& bids = IsBid ? target_bids : target_asks;
        std::map<int, int> new_qty_by_px;
        for (size_t i = 0; i < std::min(static_cast<int>(bids.size()), max_levels); ++i) {
            const Level& level = bids[i];
            assert(level.qty > 0);
            new_qty_by_px[level.px] += level.qty;
        }
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
            return false;  // Continue iteration
        }

        // Place new orders
        auto postOrder = [this, &old_qty_by_px](const auto& pair) {
            Direction direction = IsBid ? Direction::Buy : Direction::Sell;
            const auto [px, qty] = pair;
            int place_qty = qty - old_qty_by_px[px];
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
        // Update quotes on huge price change
        if (target_bids.empty()) {
            UpdateTargetQuotesOnPriceChange<false>();
        }
        if (target_asks.empty()) {
            UpdateTargetQuotesOnPriceChange<true>();
        }
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
        InitializeTargetQuotes<true>(false);
        InitializeTargetQuotes<false>(true);
        // Post initial orders
        PostOrders();
    }

    void OnOrderBookUpdate() override {
        m_logger->trace("OrderBook update.\tbid_px={}; ask_px={}", m_order_book.bid.px[0], m_order_book.ask.px[0]);
        PostOrders();
    }

    void OnTradesUpdate() override {
        m_logger->trace("OrderBook update.\tbid_px={}; ask_px={}. Trade: {}", m_order_book.bid.px[0], m_order_book.ask.px[0], m_trades);
        PostOrders();
    }

    void OnOurTrade(const LimitOrder& order, int executed_qty) override {
        m_logger->info("Execution: qty={} on order={}", executed_qty, order);
        m_logger->info("money={}; qty={}; n_orders={}", m_positions.money, m_positions.qty, m_positions.orders.size());
        if (order.direction == Direction::Buy) {
            UpdateTargetQuotesOnExecution<true>(order.px, executed_qty);
        } else {
            UpdateTargetQuotesOnExecution<false>(order.px, executed_qty);
        }
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

// Quotes Validator
struct TargetQuotesValidator {
    GridTrading& s;
    std::string msg;
    bool final_validate;
    TargetQuotesValidator(GridTrading& s, std::string msg, bool initial_validate = true, bool final_validate = true)
        : s(s), msg(std::move(msg)), final_validate(final_validate) {
        s.m_target_bid_ask_logger->info("before,{},{},{},{}", current_time(), this->msg, s.target_bids, s.target_asks);
        if (initial_validate) {
            s.ValidateTargetQuotes();
        }
    }
    ~TargetQuotesValidator() {
        s.m_target_bid_ask_logger->info("after,{},{},{},{}", current_time(), this->msg, s.target_bids, s.target_asks);
        if (final_validate) {
            s.ValidateTargetQuotes();
        }
    }
};
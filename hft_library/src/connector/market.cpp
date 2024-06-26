#include "connector/market.h"

#include <ctime>
#include <iomanip>
#include <iostream>

#include "connector/utils.h"
#include "runner.h"

template <bool IsBidParameter>
OneSideMarketOrderBook<IsBidParameter>::OneSideMarketOrderBook(int depth) : depth(depth) {}

MarketOrderBook::MarketOrderBook(const Instrument& instrument, int depth)
    : bid(depth),
      ask(depth),
      m_instrument(instrument),
      depth(depth) {
    assert(depth >= 1);
    assert(depth <= MAX_DEPTH);
}

void print_n_characters(std::ostream& os, char c, size_t n) {
    os << std::string(n, c);
}

std::ostream& operator<<(std::ostream& os, const MarketOrderBook& ob) {
    constexpr size_t TOTAL_NUMBER_OF_SPACES_IN_LINE = 4 * NUMBER_OF_SPACES_PER_NUMBER + 3 * 2 + 2;
    print_n_characters(os, '=', TOTAL_NUMBER_OF_SPACES_IN_LINE);
    os << '\n';
    print_n_characters(os, ' ', NUMBER_OF_SPACES_PER_NUMBER - 1);
    os << "Bids";
    print_n_characters(os, ' ', NUMBER_OF_SPACES_PER_NUMBER * 2 + 1);
    os << "Asks\n";
    for (int i = 0; i < ob.depth; ++i) {
        os << '[' << std::setw(NUMBER_OF_SPACES_PER_NUMBER)
           << ob.bid.px[i] << ' ' << std::setw(NUMBER_OF_SPACES_PER_NUMBER)
           << ob.bid.qty[i] << "]  ";
        os << '[' << std::setw(NUMBER_OF_SPACES_PER_NUMBER)
           << ob.ask.px[i] << ' ' << std::setw(NUMBER_OF_SPACES_PER_NUMBER)
           << ob.ask.qty[i] << "]\n";
    }
    print_n_characters(os, '=', TOTAL_NUMBER_OF_SPACES_IN_LINE);
    return os;
}

template <bool IsBidParameter>
OneSideMarketOrderBook<IsBidParameter>& MarketOrderBook::GetOneSideOrderBook() {
    if constexpr (IsBidParameter) {
        return bid;
    } else {
        return ask;
    }
}

template <bool IsBidParameter>
void MarketOrderBook::Update(const int* px, const int* qty) {
    // TODO: Add OrderBook difference computation
    OneSideMarketOrderBook<IsBidParameter>& order_book = GetOneSideOrderBook<IsBidParameter>();
    for (int i = 0; i < depth; ++i) {
        order_book.px[i] = px[i];
        order_book.qty[i] = qty[i];  // already in lots
    }
}

Trades::Trades(Instrument const& instrument) : m_instrument(instrument) {}

std::ostream& operator<<(std::ostream& os, const Trades& trades) {
    if (!trades.has_trade) {
        os << "No trades yet";
        return os;
    }
    os << "Trade: "
       << trades.last_trade.time << "  "
       << trades.last_trade.direction << "  "
       << '['
       << std::setw(NUMBER_OF_SPACES_PER_NUMBER) << trades.last_trade.px << "  "
       << std::setw(NUMBER_OF_SPACES_PER_NUMBER) << trades.last_trade.qty
       << ']';
    return os;
}

void Trades::Update(TimeType time, Direction direction, int px, int qty) {
    this->has_trade = true;
    this->last_trade = MarketTrade{
        .time = time,
        .direction = direction,
        .px = px,
        .qty = qty};
}

MarketConnector::MarketConnector(Runner& runner, const ConfigType& config)
    : m_runner(runner),
      m_client(runner.GetClient()),
      m_logger(runner.GetLogger("market", false)),
      m_trades_logger(runner.GetLogger("trades", true)),
      m_orderbook_logger(runner.GetLogger("orderbook", true)),
      m_instrument(runner.GetInstrument()),
      m_order_book(m_instrument, config["market"]["depth"].as<int>()),
      m_trades(m_instrument) {
    m_trades_logger->info("strategy_time,exchange_time,direction,px,qty");
    std::string order_book_header = "strategy_time,exchange_time";
    for (size_t i = 0; i < m_order_book.depth; ++i) {
        order_book_header += fmt::format(",bid_px_{},bid_qty_{},ask_px_{},ask_qty_{}", i, i, i, i);
    }
    m_orderbook_logger->info(order_book_header);
}

const MarketOrderBook& MarketConnector::GetOrderBook() const { return m_order_book; }

const Trades& MarketConnector::GetTrades() const { return m_trades; }

void MarketConnector::Start() {
    m_logger->info("Start MarketConnector");

    // Create MarketDataStream
    m_market_data_stream = std::dynamic_pointer_cast<MarketDataStream>(m_client.service("marketdatastream"));

    // Subscribe OrderBookStream
    m_market_data_stream->SubscribeOrderBookAsync(
        {m_instrument.figi},
        m_order_book.depth,
        [this](ServiceReply reply) {
            this->OrderBookStreamCallBack(ParseReply<MarketDataResponse>(reply, m_logger));
        });

    // Subscribe TradeStream
    m_market_data_stream->SubscribeTradesAsync(
        {m_instrument.figi},
        [this](ServiceReply reply) {
            this->TradeStreamCallBack(ParseReply<MarketDataResponse>(reply, m_logger));
        });
}

void MarketConnector::OrderBookStreamCallBack(MarketDataResponse* response) {
    if (response->has_subscribe_order_book_response()) {
        // Process Start of subscription
        const google::protobuf::RepeatedPtrField<OrderBookSubscription>& subscriptions = response->subscribe_order_book_response().order_book_subscriptions();
        assert(subscriptions.size() == 1);
        assert(subscriptions[0].subscription_status() == SubscriptionStatus::SUBSCRIPTION_STATUS_SUCCESS);
        m_logger->info("OrderBookStream subscribe: success. depth={}", m_order_book.depth);
    } else if (response->has_orderbook()) {
        LockGuard lock = m_runner.GetEventLock();
        // Process subscription message
        const OrderBook& order_book = response->orderbook();
        assert(order_book.depth() == m_order_book.depth);
        assert(order_book.figi() == m_instrument.figi);
        // TODO: parse time

        // Parse bids and asks
        int px[MAX_DEPTH];
        int qty[MAX_DEPTH];
        m_order_book.time = time_from_protobuf(order_book.time());

        ParseLevels(order_book.bids(), px, qty);
        m_order_book.Update<true>(px, qty);

        ParseLevels(order_book.asks(), px, qty);
        m_order_book.Update<false>(px, qty);

        assert(m_order_book.bid.px[0] < m_order_book.ask.px[0]);

        // Log the order book data
        fmt::memory_buffer buf;
        fmt::format_to(std::back_inserter(buf), "{},{}", current_time(), m_order_book.time);
        for (size_t i = 0; i < m_order_book.depth; ++i) {
            fmt::format_to(std::back_inserter(buf), ",{},{},{},{}", m_order_book.bid.px[i], m_order_book.bid.qty[i], m_order_book.ask.px[i], m_order_book.ask.qty[i]);
        }
        m_orderbook_logger->info("{}", fmt::to_string(buf));

        if (!m_is_order_book_stream_ready) {
            // Notify strategy about connector readiness
            m_is_order_book_stream_ready = true;
            if (this->IsReady()) m_runner.OnMarketConnectorReady();
        } else {
            // Notify strategy
            if (lock.NotifyNow()) {
                m_runner.OnOrderBookUpdate();
            } else {
                m_logger->info("Skip OrderBook notification: {} events pending", lock.GetNumberEventsPending());
            }
        }
    } else {
        // Process ping
        assert(response->has_ping());
    }
}

void MarketConnector::TradeStreamCallBack(MarketDataResponse* response) {
    if (response->has_subscribe_trades_response()) {
        // Process Start of subscription
        const google::protobuf::RepeatedPtrField<TradeSubscription>& subscriptions = response->subscribe_trades_response().trade_subscriptions();
        assert(subscriptions.size() == 1);
        assert(subscriptions[0].subscription_status() == SubscriptionStatus::SUBSCRIPTION_STATUS_SUCCESS);
        m_logger->info("TradeStream subscribe: success");
        // Notify strategy about connector readiness
        m_is_trade_stream_ready = true;
        if (this->IsReady()) m_runner.OnMarketConnectorReady();
    } else if (response->has_trade()) {
        LockGuard lock = m_runner.GetEventLock();
        // Process subscription message
        const Trade& trade = response->trade();
        assert(trade.figi() == m_instrument.figi);
        // TODO: parse time

        // Parse Trade
        const int direction = trade.direction();
        assert(direction == TradeDirection::TRADE_DIRECTION_BUY || direction == TradeDirection::TRADE_DIRECTION_SELL);
        m_trades.Update(
            time_from_protobuf(trade.time()),
            direction == TradeDirection::TRADE_DIRECTION_BUY ? Direction::Buy : Direction::Sell,
            m_instrument.QuotationToPx(trade.price()),
            static_cast<int>(trade.quantity()));
        m_trades_logger->info("{},{},{},{},{}", current_time(), m_trades.last_trade.time, m_trades.last_trade.direction, m_trades.last_trade.px, m_trades.last_trade.qty);

        // Notify strategy
        if (lock.NotifyNow()) {
            m_runner.OnTradesUpdate();
        } else {
            m_logger->info("Skip Trades notification: {} events pending", lock.GetNumberEventsPending());
        }
    } else {
        // Process ping
        assert(response->has_ping());
    }
}

bool MarketConnector::IsReady() const {
    return m_is_order_book_stream_ready & m_is_trade_stream_ready;
}

void MarketConnector::ParseLevels(const google::protobuf::RepeatedPtrField<Order>& orders, int* px, int* qty) const {
    if (orders.size() == 0) {
        throw std::runtime_error("Empty orderbook. Probably, the trading session is closed");
    }
    assert(orders.size() == m_order_book.depth);
    for (int i = 0; i < m_order_book.depth; ++i) {
        const Order& order = orders[i];
        px[i] = m_instrument.QuotationToPx(order.price());
        qty[i] = static_cast<int>(order.quantity());
    }
}
#include <connector/market.h>
#include <runner.h>

#include <iostream>

MarketOrderBook::MarketOrderBook(const Instrument& instrument, int depth)
        :
        bid(depth),
        ask(depth),
        m_instrument(instrument),
        depth(depth) {
    assert(depth >= 1);
    assert(depth <= MAX_DEPTH);
}

template <bool IsBidParameter>
OneSideMarketOrderBook<IsBidParameter>::OneSideMarketOrderBook(int depth) : depth(depth) {}

void print_n_characters(char c, int n) {
    for (int i = 0; i < n; ++i) {
        std::cout << c;
    }
}

template <bool IsBidParameter>
constexpr bool OneSideMarketOrderBook<IsBidParameter>::IsBid() { return IsBidParameter; }

void MarketOrderBook::Print() const {
    constexpr int TOTAL_NUMBER_OF_SPACES_IN_LINE = 4 * NUMBER_OF_SPACES_PER_NUMBER + 3 * 2 + 2;
    print_n_characters('=', TOTAL_NUMBER_OF_SPACES_IN_LINE);
    std::cout << std::endl;
    print_n_characters(' ', NUMBER_OF_SPACES_PER_NUMBER - 1);
    std::cout << "Bids";
    print_n_characters(' ', NUMBER_OF_SPACES_PER_NUMBER * 2 + 1);
    std::cout << "Asks" << std::endl;
    for (int i = 0; i < depth; ++i) {
        std::cout << '[' << std::setw(NUMBER_OF_SPACES_PER_NUMBER)
                  << bid.px[i] << ' ' << std::setw(NUMBER_OF_SPACES_PER_NUMBER)
                  << bid.qty[i] << "]  ";
        std::cout << '[' << std::setw(NUMBER_OF_SPACES_PER_NUMBER)
                  << ask.px[i] << ' ' << std::setw(NUMBER_OF_SPACES_PER_NUMBER)
                  << ask.qty[i] << ']' << std::endl;
    }
    print_n_characters('=', TOTAL_NUMBER_OF_SPACES_IN_LINE);
    std::cout << std::endl;
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
void MarketOrderBook::Update(const double* px, const int* qty) {
    // TODO: Add OrderBook difference computation
    OneSideMarketOrderBook<IsBidParameter>& order_book = GetOneSideOrderBook<IsBidParameter>();
    for (int i = 0; i < depth; ++i) {
        order_book.px[i] = m_instrument.DoublePxToInt(px[i]);
        order_book.qty[i] = qty[i]; // already in lots
    }
}

Trades::Trades(Instrument const& instrument) : m_instrument(instrument) {}

void Trades::Print() const {
    if (!has_trade) {
        std::cout << "No trades yet" << std::endl;
        return;
    }
    std::cout << "Trade: "
              << last_trade.direction << "  "
              << '['
              << std::setw(NUMBER_OF_SPACES_PER_NUMBER) << last_trade.px << "  "
              << std::setw(NUMBER_OF_SPACES_PER_NUMBER) << last_trade.qty
              << ']' << std::endl;
}

void Trades::Update(Direction direction, double px, int qty) {
    this->has_trade = true;
    this->last_trade = MarketTrade {
            .direction = direction,
            .px = m_instrument.DoublePxToInt(px),
            .qty = qty
    };
}

MarketConnector::MarketConnector(Runner& runner, const ConfigType& config, InvestApiClient& client, const Instrument& instrument)
        :
        m_runner(runner),
        m_client(client),
        m_instrument(instrument),
        m_order_book(instrument, config["market"]["depth"].as<int>()),
        m_trades(instrument) {}

const MarketOrderBook& MarketConnector::GetOrderBook() const { return m_order_book; }

const Trades& MarketConnector::GetTrades() const { return m_trades; }

void MarketConnector::Start() {
    std::cout << "Start MarketConnector" << std::endl;

    // Create MarketDataStream
    m_market_data_stream = std::dynamic_pointer_cast<MarketDataStream>(m_client.service("marketdatastream"));

    // Subscribe OrderBookStream
    m_market_data_stream->SubscribeOrderBookAsync(
            {m_instrument.figi},
            m_order_book.depth,
            [this](ServiceReply reply) {
                this->OrderBookStreamCallBack(std::move(reply));
            }
    );

    // Subscribe TradeStream
    m_market_data_stream->SubscribeTradesAsync(
            {m_instrument.figi},
            [this](ServiceReply reply) {
                this->TradeStreamCallBack(std::move(reply));
            }
    );
}

void MarketConnector::OrderBookStreamCallBack(ServiceReply reply) {
    CheckReply(reply);
    auto response = dynamic_pointer_cast<MarketDataResponse>(reply.ptr());
    assert(response);
    if (response->has_subscribe_order_book_response()) {
        // Process Start of subscription
        const google::protobuf::RepeatedPtrField<OrderBookSubscription>& subscriptions = response->subscribe_order_book_response().order_book_subscriptions();
        assert(subscriptions.size() == 1);
        assert(subscriptions[0].subscription_status() == SubscriptionStatus::SUBSCRIPTION_STATUS_SUCCESS);
        std::cout << "OrderBookStream subscribe: success" << std::endl;
        if (!m_is_order_book_stream_ready) {
            m_is_order_book_stream_ready = true;
            if (this->IsReady()) m_runner.OnMarketConnectorReady();
        }
    } else if (response->has_orderbook()) {
        // Process subscription message
        const OrderBook& order_book = response->orderbook();
        assert(order_book.depth() == m_order_book.depth);
        assert(order_book.figi() == m_instrument.figi);
        // TODO: parse time

        // Parse bids and asks
        double px[MAX_DEPTH];
        int qty[MAX_DEPTH];

        ParseLevels(order_book.bids(), px, qty);
        m_order_book.Update<true>(px, qty);

        ParseLevels(order_book.asks(), px, qty);
        m_order_book.Update<false>(px, qty);

        // Notify strategy
        m_runner.OnOrderBookUpdate();
    } else {
        // Process ping
        assert(response->has_ping());
    }
}

void MarketConnector::TradeStreamCallBack(ServiceReply reply) {
    CheckReply(reply);
    auto response = dynamic_pointer_cast<MarketDataResponse>(reply.ptr());
    assert(response);
    if (response->has_subscribe_trades_response()) {
        // Process Start of subscription
        const google::protobuf::RepeatedPtrField<TradeSubscription>& subscriptions = response->subscribe_trades_response().trade_subscriptions();
        assert(subscriptions.size() == 1);
        assert(subscriptions[0].subscription_status() == SubscriptionStatus::SUBSCRIPTION_STATUS_SUCCESS);
        std::cout << "TradeStream subscribe: success" << std::endl;
        if (!m_is_trade_stream_ready) {
            m_is_trade_stream_ready = true;
            if (this->IsReady()) m_runner.OnMarketConnectorReady();
        }
    } else if (response->has_trade()) {
        // Process subscription message
        const Trade& trade = response->trade();
        assert(trade.figi() == m_instrument.figi);
        // TODO: parse time

        // Parse Trade
        // Buy: 1
        // Sell: 2
        const int direction = trade.direction();
        assert(direction == 1 || direction == 2);
        m_trades.Update(
                direction == 1 ? Direction::Buy : Direction::Sell,
                QuotationToDouble(trade.price()),
                static_cast<int>(trade.quantity())
        );

        // Notify strategy
        m_runner.OnTradesUpdate();
    } else {
        // Process ping
        assert(response->has_ping());
    }
}

bool MarketConnector::IsReady() const {
    return m_is_order_book_stream_ready & m_is_trade_stream_ready;
}

void MarketConnector::ParseLevels(const google::protobuf::RepeatedPtrField<Order>& orders, double* px, int* qty) const {
    assert(orders.size() == m_order_book.depth);
    for (int i = 0; i < m_order_book.depth; ++i) {
        const Order& order = orders[i];
        px[i] = QuotationToDouble(order.price());
        qty[i] = static_cast<int>(order.quantity());
    }
}
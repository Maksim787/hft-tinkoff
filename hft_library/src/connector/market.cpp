#include <connector/market.h>
#include <connector/utils.h>
#include <runner.h>

#include <iostream>

MarketOrderBook::MarketOrderBook(const InstrumentInfo& instrument, int depth)
        :
        m_instrument(instrument),
        depth(depth) {
    assert(depth >= 1);
    assert(depth <= MAX_DEPTH);
}

void MarketOrderBook::Update(bool is_bid, double* px, int* qty) {
    // TODO: Add OrderBook difference computation
    OneSideMarketOrderBook order_book = is_bid ? bid : ask;
    for (int i = 0; i < depth; ++i) {
        order_book.px[i] = std::round(px[i] / m_instrument.px_step);
        if (is_bid) {
            order_book.px[i] *= -1;
        }
        order_book.qty[i] = qty[i]; // already in lots
    }
}

Trades::Trades(InstrumentInfo const& instrument) : m_instrument(instrument) {}

void Trades::Update(Direction direction, double px, int qty) {
    this->has_trade = true;
    this->last_trade = MarketTrade {
            .direction = direction,
            .px = static_cast<int>(std::round(px / m_instrument.px_step)),
            .qty = qty
    };
}

MarketConnector::MarketConnector(Runner& runner, const ConfigType& config, InvestApiClient& client, const InstrumentInfo& instrument)
        :
        m_runner(runner),
        m_client(client),
        m_instrument(instrument),
        m_order_book(instrument, config["market"]["depth"].as<int>()),
        m_trades(instrument) {}

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
    // Process Start of subscription
    auto response = dynamic_pointer_cast<MarketDataResponse>(reply.ptr());
    assert(response);
    if (response->has_subscribe_order_book_response()) {
        const google::protobuf::RepeatedPtrField<OrderBookSubscription>& subscriptions = response->subscribe_order_book_response().order_book_subscriptions();
        assert(subscriptions.size() == 1);
        assert(subscriptions[0].subscription_status() == SubscriptionStatus::SUBSCRIPTION_STATUS_SUCCESS);
        std::cout << "OrderBookStream subscribe: success" << std::endl;
        if (!m_is_order_book_stream_ready) {
            m_is_order_book_stream_ready = true;
            if (this->IsReady()) m_runner.OnMarketConnectorReady();
        }
        return;
    }
    // Process subscription message
    if (response->has_orderbook()) {
        const OrderBook& order_book = response->orderbook();
        assert(order_book.depth() == m_order_book.depth);
        assert(order_book.figi() == m_instrument.figi);
        // TODO: parse time

        // Parse bids and asks
        double px[MAX_DEPTH];
        int qty[MAX_DEPTH];

        ParseLevels(order_book.bids(), px, qty);
        m_order_book.Update(true, px, qty);

        ParseLevels(order_book.asks(), px, qty);
        m_order_book.Update(false, px, qty);

        return;
    }
    // Process ping
    assert(response->has_ping());
}

void MarketConnector::TradeStreamCallBack(ServiceReply reply) {
    CheckReply(reply);
    // Process Start of subscription
    auto response = dynamic_pointer_cast<MarketDataResponse>(reply.ptr());
    assert(response);
    if (response->has_subscribe_trades_response()) {
        const google::protobuf::RepeatedPtrField<TradeSubscription>& subscriptions = response->subscribe_trades_response().trade_subscriptions();
        assert(subscriptions.size() == 1);
        assert(subscriptions[0].subscription_status() == SubscriptionStatus::SUBSCRIPTION_STATUS_SUCCESS);
        std::cout << "TradeStream subscribe: success" << std::endl;
        if (!m_is_trade_stream_ready) {
            m_is_trade_stream_ready = true;
            if (this->IsReady()) m_runner.OnMarketConnectorReady();
        }
        return;
    }
    // Process subscription message
    if (response->has_trade()) {
        const Trade& trade = response->trade();
        assert(trade.figi() == m_instrument.figi);
        // TODO: parse time

        // Parse Trade
        // Buy: 2 -> 1
        // Sell: 1 -> -1
        int direction = trade.direction();
        assert(direction == 1 || direction == 2);
        m_trades.Update(direction == 2 ? Direction::Buy : Direction::Sell, QuotationToDouble(trade.price()), trade.quantity());
        return;
    }
    // Process ping
    assert(response->has_ping());
}

bool MarketConnector::IsReady() {
    return m_is_order_book_stream_ready & m_is_trade_stream_ready;
}

void MarketConnector::ParseLevels(const google::protobuf::RepeatedPtrField<Order>& orders, double* px, int* qty) {
    assert(orders.size() == m_order_book.depth);
    for (int i = 0; i < m_order_book.depth; ++i) {
        const Order& order = orders[i];
        px[i] = QuotationToDouble(order.price());
        qty[i] = order.quantity();
    }
}


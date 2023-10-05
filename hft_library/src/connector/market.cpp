#include <connector/market.h>
#include <runner.h>

#include <iostream>

MarketConnector::MarketConnector(Runner& runner, const ConfigType& config, InvestApiClient& client)
        :
        m_runner(runner),
        m_client(client),
        m_figi(config["runner"]["figi"].as<std::string>()),
        m_order_book_depth(config["market"]["depth"].as<int>()) {}

void MarketConnector::Start() {
    std::cout << "Start MarketConnector" << std::endl;

    // Create MarketDataStream
    m_market_data_stream = std::dynamic_pointer_cast<MarketDataStream>(m_client.service("marketdatastream"));

    // Subscribe OrderBookStream
    m_market_data_stream->SubscribeOrderBookAsync(
            {m_figi},
            m_order_book_depth,
            [this](ServiceReply reply) {
                this->OrderBookStreamCallBack(std::move(reply));
            }
    );

    // Subscribe TradeStream
    m_market_data_stream->SubscribeTradesAsync(
            {m_figi},
            [this](ServiceReply reply) {
                this->TradeStreamCallBack(std::move(reply));
            }
    );
}

void MarketConnector::OrderBookStreamCallBack(ServiceReply reply) {
    // TODO: check readiness
    if (!m_is_order_book_stream_ready) {
        m_is_order_book_stream_ready = true;
        if (this->IsReady()) m_runner.OnMarketConnectorReady();
    }
    std::cout << reply.ptr()->DebugString() << std::endl;
}

void MarketConnector::TradeStreamCallBack(ServiceReply reply) {
    // TODO: check readiness
    if (!m_is_trade_stream_ready) {
        m_is_trade_stream_ready = true;
        if (this->IsReady()) m_runner.OnMarketConnectorReady();
    }
    std::cout << reply.ptr()->DebugString() << std::endl;
}

bool MarketConnector::IsReady() {
    return m_is_order_book_stream_ready & m_is_trade_stream_ready;
}
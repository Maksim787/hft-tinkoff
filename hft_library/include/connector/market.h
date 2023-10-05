#pragma once

#include <constants.h>

#include <investapiclient.h>
#include <marketdatastreamservice.h>
//#include <runner.h>


class Runner;

class MarketConnector {
private:
    // Runner
    Runner& m_runner;
    // Client
    InvestApiClient& m_client;
    // Instrument
    std::string m_figi;
    int m_order_book_depth;
    // MarketDataStream
    std::shared_ptr<MarketDataStream> m_market_data_stream;
    // Readiness
    bool m_is_order_book_stream_ready = false;
    bool m_is_trade_stream_ready = false;
public:
    MarketConnector(Runner& runner, const ConfigType& config, InvestApiClient& client);

private:
    // Methods for Runner
    friend class Runner;

    void Start();

    // Methods for MarketConnector
    void OrderBookStreamCallBack(ServiceReply reply);

    void TradeStreamCallBack(ServiceReply reply);

    bool IsReady();
};

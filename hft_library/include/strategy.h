#pragma once

#include <config.h>
#include <connector/market.h>
#include <connector/user.h>

class Runner;

class Strategy {
protected:
    // Runner and connectors
    Runner& m_runner;
    MarketConnector& m_mkt;
    UserConnector& m_usr;

    // Config and useful fields
    const Instrument& m_instrument;
    const ConfigType& m_config;
    const MarketOrderBook& m_order_book;
    const Trades& m_trades;

public:
    Strategy(Runner& runner);

private:
    friend class Runner;

    friend class MarketConnector;

    friend class UserConnector;

    virtual void OnConnectorsReadiness() = 0;

    virtual void OnOrderBookUpdate() = 0;

    virtual void OnTradesUpdate() = 0;
};
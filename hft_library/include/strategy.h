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

    // Config and instrument
    const Instrument& m_instrument;
    const ConfigType& m_config;

    // Market data
    const MarketOrderBook& m_order_book;
    const Trades& m_trades;

    // User data
    const Positions& m_positions;

public:
    Strategy(Runner& runner);

private:
    friend class Runner;

    friend class MarketConnector;

    friend class UserConnector;

    // Runner methods
    virtual void OnConnectorsReadiness() = 0;

    // Market Connector methods
    virtual void OnOrderBookUpdate() = 0;

    virtual void OnTradesUpdate() = 0;

    // user Connector methods
    virtual void OnOurTrade(const LimitOrder& order, int executed_qty) = 0;
};
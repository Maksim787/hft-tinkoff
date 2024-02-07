#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include "config.h"
#include "connector/utils.h"
#include "connector/user.h"
#include "connector/market.h"

class Runner;

class Strategy {
protected:
    // Runner and connectors
    Runner& m_runner;
    std::shared_ptr<spdlog::logger> m_logger;

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

    // TODO: OnPingUpdate()

    // Runner methods
    virtual void OnConnectorsReadiness() = 0;

    // Market Connector methods
    virtual void OnOrderBookUpdate() = 0;

    virtual void OnTradesUpdate() = 0;

    // User Connector methods
    virtual void OnOurTradeAsync(const LimitOrder& order, int executed_qty) {};

    virtual void OnOurTrade(const LimitOrder& order, int executed_qty) = 0;
};
#pragma once

#include <connector/market.h>
#include <connector/user.h>
#include <connector/utils.h>
#include <strategy.h>
#include <config.h>

#include <functional>

class Runner {
private:
    // Config
    ConfigType m_config;

    // Client for connectors
    InvestApiClient m_client;

    // Instrument
    Instrument m_instrument;

    // Connectors
    MarketConnector m_mkt;
    UserConnector m_usr;

    // Readiness
    bool m_is_mkt_ready = false;
    bool m_is_usr_ready = false;

    // Strategy
    std::shared_ptr<Strategy> m_strategy;

public:
    using StrategyGetter = std::function<std::shared_ptr<Strategy>(Runner&)>;

    Runner(const ConfigType& config, const StrategyGetter& strategy_getter);

    // Start all connectors
    void Start();

    // Getters
    const ConfigType& GetConfig() const;

    const Instrument& GetInstrument() const;

    MarketConnector& GetMarketConnector();

    UserConnector& GetUserConnector();

private:
    friend class MarketConnector;

    // Methods for MarketConnector
    void OnMarketConnectorReady();

    void OnOrderBookUpdate();

    void OnTradesUpdate();

    friend class UserConnector;

    // Methods for UserConnector
    void OnUserConnectorReady();

    void OnOurTrade(const LimitOrder& order, int executed_qty);

    // Methods for Runner
    bool IsReady();
};
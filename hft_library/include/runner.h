#pragma once

#include <config.h>
#include <connector/market.h>
#include <connector/user.h>
#include <connector/utils.h>

class Runner {
private:
    // Client for connectors
    InvestApiClient m_client;

    // Instrument
    InstrumentInfo m_instrument;

    // Connectors
    MarketConnector m_mkt;
    UserConnector m_usr;

    // Readiness
    bool m_is_mkt_ready = false;
    bool m_is_usr_ready = false;

public:
    Runner(const ConfigType& config);

    void Start();

private:
    friend class MarketConnector;

    // Methods for MarketConnector
    void OnMarketConnectorReady();

    friend class UserConnector;

    // Methods for UserConnector
    void OnUserConnectorReady();

    // Methods for Runner
    bool IsReady();
};
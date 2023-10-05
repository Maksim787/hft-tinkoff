#pragma once

#include <constants.h>

#include <investapiclient.h>
#include <ordersstreamservice.h>

class Runner;

class UserConnector {
private:
    // Runner
    Runner& m_runner;
    // Client
    InvestApiClient& m_client;
    // Instrument
    std::string m_figi;
    // OrdersStream
    std::shared_ptr<OrdersStream> m_orders_stream;
    // Readiness
    bool m_is_order_stream_ready = false;

public:
    UserConnector(Runner& runner, const ConfigType& config, InvestApiClient& client);

private:
    // Methods for Runner
    friend class Runner;

    void Start();

    // Methods for UserConnector
    void OrderStreamCallback(ServiceReply reply);

    bool IsReady();
};

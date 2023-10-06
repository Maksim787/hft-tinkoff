#pragma once

#include <constants.h>
#include <connector/utils.h>

#include <investapiclient.h>
#include <ordersstreamservice.h>

class Runner;

class Strategy;

class UserConnector {
private:
    // Runner
    Runner& m_runner;
    // Strategy
    std::shared_ptr<Strategy> m_strategy = nullptr;
    // Client
    InvestApiClient& m_client;
    // Instrument
    const Instrument& m_instrument;
    // OrdersStream
    std::shared_ptr<OrdersStream> m_orders_stream;
    // Readiness
    bool m_is_order_stream_ready = false;

public:
    UserConnector(Runner& runner, const ConfigType& config, InvestApiClient& client, const Instrument& instrument);

private:
    // Methods for Runner
    friend class Runner;

    void Start();

    // Methods for UserConnector
    void OrderStreamCallback(ServiceReply reply);

    bool IsReady();
};

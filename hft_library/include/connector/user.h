#pragma once

#include <constants.h>
#include <connector/utils.h>

#include <investapiclient.h>
#include <ordersstreamservice.h>

class Runner;

class Strategy;

class LimitOrder {
public:
    const std::string order_id;
    const Direction direction;
    const int px; // real_px / px_step
    int qty; // in lots

    void Print() const;
};

class Positions {
public:
    std::map<std::string, LimitOrder> orders; // orders by id
    int qty = 0; // > 0 if Long; < 0 if Short
    double money = 0; // real_money / (lot * px_step)

    void Print() const;
};

class UserConnector {
private:
    // Runner
    Runner& m_runner;
    // Strategy
    std::shared_ptr<Strategy> m_strategy = nullptr;
    // Client
    InvestApiClient& m_client;

    // Account
    const std::string m_account_id;
    // Instrument
    const Instrument& m_instrument;

    // OrdersStream
    std::shared_ptr<OrdersStream> m_orders_stream;

    // Readiness
    bool m_is_order_stream_ready = false;

    // Positions
    Positions m_positions;

public:
    UserConnector(Runner& runner, const ConfigType& config, InvestApiClient& client, const Instrument& instrument);

    // Getters
    const Positions& GetPositions() const;

private:
    // Methods for Runner
    friend class Runner;

    void Start();

    // Methods for UserConnector
    void OrderStreamCallback(ServiceReply reply);

    void ProcessOurTrade(const std::string& order_id, int total_qty, int px, Direction direction);

    bool IsReady() const;
};

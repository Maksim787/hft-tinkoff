#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include "hft_library/third_party/TinkoffInvestSDK/investapiclient.h"
#include "hft_library/third_party/TinkoffInvestSDK/services/ordersstreamservice.h"
#include "hft_library/third_party/TinkoffInvestSDK/services/ordersservice.h"

#include "constants.h"
#include "connector/utils.h"
#include "connector/logger.h"

class Runner;

class LockGuard;

class Strategy;

class LimitOrder {
public:
    const std::string order_id;
    const Direction direction;
    const int px; // real_px / px_step
    int qty; // in lots
};

class Positions {
public:
    std::map<std::string, LimitOrder> orders; // orders by id
    int qty = 0; // > 0 if Long; < 0 if Short
    int money = 0; // real_money / (lot * px_step)
};

std::ostream& operator<<(std::ostream& os, const LimitOrder& order);

std::ostream& operator<<(std::ostream& os, const Positions& positions);

class UserConnector {
private:
    // Runner
    Runner& m_runner;
    // Client
    InvestApiClient& m_client;
    // Logger
    std::shared_ptr<spdlog::logger> m_logger;

    std::shared_ptr<CHLoggerManager> m_ch_logger_manager;

    // Account
    const std::string m_account_id;
    // Instrument
    const Instrument& m_instrument;

    // OrdersStream: initialized in Start()
    std::shared_ptr<OrdersStream> m_orders_stream;

    // Orders service: initialized in Start()
    std::shared_ptr<Orders> m_orders_service;

    // Readiness
    bool m_is_order_stream_ready = false;

    // Positions
    Positions m_positions;

public:
    UserConnector(Runner& runner, const ConfigType& config);

    // Getters
    const Positions& GetPositions() const;

private:
    // Methods for Runner
    friend class Runner;

    void Start();

    const LimitOrder& PostOrder(int px, int qty, Direction direction);

    void CancelOrder(const std::string& order_id);

    // Methods for UserConnector
    void OrderStreamCallback(TradesStreamResponse* response);

    void ProcessOurTrade(const LockGuard& lock, const std::string& order_id, int px, int qty, Direction direction);

    const LimitOrder& ProcessNewPostOrder(const std::string& order_id, int px, int qty, Direction direction);

    bool IsReady() const;
};

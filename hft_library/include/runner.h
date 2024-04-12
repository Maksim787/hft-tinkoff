#pragma once

#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <mutex>

#include "config.h"
#include "connector/market.h"
#include "connector/user.h"
#include "connector/utils.h"
#include "strategy.h"

class Runner;

// Synchronization
class LockGuard {
    Runner& m_runner;

   public:
    bool NotifyNow() const;

    int GetNumberEventsPending() const;

    ~LockGuard();

   private:
    LockGuard(Runner& runner);

    friend class Runner;
};

class Runner {
   private:
    // Config
    ConfigType m_config;

    // Loggers
    std::map<std::string, std::shared_ptr<spdlog::logger>> m_loggers;
    std::shared_ptr<spdlog::logger> m_runner_logger;

    // Client for connectors
    InvestApiClient m_client;

    // Instrument
    Instrument m_instrument;

    std::atomic_int n_pending_events = 0;
    std::mutex m_mutex;

    // Connectors
    MarketConnector m_mkt;
    UserConnector m_usr;

    // Readiness
    bool m_is_mkt_ready = false;
    bool m_is_usr_ready = false;

    // Strategy
    std::shared_ptr<Strategy> m_strategy;  // Strategy is an abstract class

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

    std::shared_ptr<spdlog::logger> GetLogger(const std::string& name, bool only_text);

    int GetPendingEvents() const;

    // Order manipulations
    const LimitOrder& PostOrder(int px, int qty, Direction direction);

    void CancelOrder(const std::string& order_id);

   private:
    friend class MarketConnector;

    friend class LockGuard;

    // Getters for MarketConnector and UserConnector
    InvestApiClient& GetClient();

    // Methods for synchronization
    LockGuard GetEventLock();

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
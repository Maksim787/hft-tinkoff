#pragma once

#include <functional>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <connector/market.h>
#include <connector/user.h>
#include <connector/utils.h>
#include <strategy.h>
#include <config.h>


class Runner {
private:
    // Config
    ConfigType m_config;

    // Logger
    std::shared_ptr<spdlog::sinks::basic_file_sink_mt> m_file_sink;
    std::shared_ptr<spdlog::logger> m_runner_logger;
    std::shared_ptr<spdlog::logger> m_mkt_logger;
    std::shared_ptr<spdlog::logger> m_usr_logger;
    std::shared_ptr<spdlog::logger> m_strategy_logger;

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
    std::shared_ptr<Strategy> m_strategy; // Strategy is an abstract class

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

    std::shared_ptr<spdlog::logger> GetStrategyLogger();

    // Order manipulations
    const LimitOrder& PostOrder(int px, int qty, Direction direction);

    void CancelOrder(const std::string& order_id);

private:
    friend class MarketConnector;

    // Getters for MarketConnector and UserConnector
    InvestApiClient& GetClient();

    std::shared_ptr<spdlog::logger> GetMarketLogger();

    std::shared_ptr<spdlog::logger> GetUserLogger();

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
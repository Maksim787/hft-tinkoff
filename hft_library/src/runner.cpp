#include "runner.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include <filesystem>

#include "constants.h"

Runner::Runner(const ConfigType& config, const StrategyGetter& strategy_getter)
    : m_config(config),
      m_runner_logger(GetLogger("runner", false)),
      m_client(ENDPOINT, config["runner"]["token"].as<std::string>()),
      // TODO: Get/Check instrument information in RunTime
      m_instrument(
          config["runner"]["figi"].as<std::string>(),
          config["runner"]["lot_size"].as<int>(),
          config["runner"]["px_step"].as<double>()),
      m_mkt(*this, config),
      m_usr(*this, config),
      m_strategy(strategy_getter(*this)) {}

void Runner::Start() {
    m_runner_logger->info(std::string(50, '='));
    m_mkt.Start();
    m_usr.Start();
}

const ConfigType& Runner::GetConfig() const {
    return m_config;
}

const Instrument& Runner::GetInstrument() const {
    return m_instrument;
}

MarketConnector& Runner::GetMarketConnector() {
    return m_mkt;
}

UserConnector& Runner::GetUserConnector() {
    return m_usr;
}

InvestApiClient& Runner::GetClient() {
    return m_client;
}

std::shared_ptr<spdlog::logger> Runner::GetLogger(const std::string& name, bool only_text) {
    auto it = m_loggers.find(name);
    if (it != m_loggers.end()) {
        // Logger exists
        return it->second;
    }
    // Create file
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(std::filesystem::path(m_config["runner"]["log_directory"].as<std::string>()) / (name + ".txt"), false);
    if (only_text) {
        file_sink->set_pattern("%v");
    }
    // Create logger
    auto logger = std::make_shared<spdlog::logger>(name, only_text ? spdlog::sinks_init_list{file_sink} : spdlog::sinks_init_list{file_sink, std::make_shared<spdlog::sinks::stdout_sink_mt>()});
    // Configure logger
    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::trace);
    spdlog::register_logger(logger);
    // Store logger
    m_loggers[name] = logger;
    // Return logger
    return logger;
}

int Runner::GetPendingEvents() const {
    return n_pending_events - 1;
}

const LimitOrder& Runner::PostOrder(int px, int qty, Direction direction) {
    return m_usr.PostOrder(px, qty, direction);
}

void Runner::CancelOrder(const std::string& order_id) {
    m_usr.CancelOrder(order_id);
}

void Runner::OnMarketConnectorReady() {
    m_is_mkt_ready = true;
    m_runner_logger->info("MarketConnector is Ready");
    if (IsReady()) m_strategy->OnConnectorsReadiness();
}

void Runner::OnOrderBookUpdate() {
    // Notify only if all connectors are ready
    if (IsReady()) m_strategy->OnOrderBookUpdate();
}

void Runner::OnTradesUpdate() {
    // Notify only if all connectors are ready
    if (IsReady()) m_strategy->OnTradesUpdate();
}

void Runner::OnUserConnectorReady() {
    m_is_usr_ready = true;
    m_runner_logger->info("UserConnector is Ready");
    if (IsReady()) m_strategy->OnConnectorsReadiness();
}

void Runner::OnOurTrade(const LimitOrder& order, int executed_qty) {
    // Always notify
    assert(IsReady() && "Connectors should be ready before order processing");
    m_strategy->OnOurTrade(order, executed_qty);
}

bool Runner::IsReady() {
    return m_is_mkt_ready & m_is_usr_ready;
}

LockGuard Runner::GetEventLock() {
    return LockGuard(*this);
}

bool LockGuard::NotifyNow() const {
    assert(m_runner.n_pending_events >= 1);
    return m_runner.n_pending_events == 1;
}

int LockGuard::GetNumberEventsPending() const {
    return m_runner.n_pending_events - 1;
}

LockGuard::LockGuard(Runner& runner) : m_runner(runner) {
    assert(m_runner.n_pending_events >= 0);
    ++m_runner.n_pending_events;
    m_runner.m_mutex.lock();
}

LockGuard::~LockGuard() {
    --m_runner.n_pending_events;
    m_runner.m_mutex.unlock();
}
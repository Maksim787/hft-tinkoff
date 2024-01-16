#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <runner.h>
#include <constants.h>

Runner::Runner(const ConfigType& config, const StrategyGetter& strategy_getter)
        :
        m_config(config),
        m_file_sink(std::make_shared<spdlog::sinks::basic_file_sink_mt>(config["runner"]["log_file"].as<std::string>(), false)),
        m_runner_logger(std::make_shared<spdlog::logger>("Runner", spdlog::sinks_init_list {m_file_sink, std::make_shared<spdlog::sinks::stdout_sink_mt>()})),
        m_mkt_logger(std::make_shared<spdlog::logger>("Market", spdlog::sinks_init_list {m_file_sink, std::make_shared<spdlog::sinks::stdout_sink_mt>()})),
        m_usr_logger(std::make_shared<spdlog::logger>("User", spdlog::sinks_init_list {m_file_sink, std::make_shared<spdlog::sinks::stdout_sink_mt>()})),
        m_strategy_logger(std::make_shared<spdlog::logger>("Strategy", spdlog::sinks_init_list {m_file_sink, std::make_shared<spdlog::sinks::stdout_sink_mt>()})),
        m_client(ENDPOINT, config["runner"]["token"].as<std::string>()),
        // TODO: Get/Check instrument information in RunTime
        m_instrument(
                config["runner"]["figi"].as<std::string>(),
                config["runner"]["lot_size"].as<int>(),
                config["runner"]["px_step"].as<double>()
        ),
        m_mkt(*this, config),
        m_usr(*this, config),
        m_strategy(strategy_getter(*this)) {
    // Configure loggers
    for (std::shared_ptr<spdlog::logger> logger: {m_runner_logger, m_mkt_logger, m_usr_logger, m_strategy_logger}) {
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::trace);
        spdlog::register_logger(logger);
    }
}

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

std::shared_ptr<spdlog::logger> Runner::GetMarketLogger() {
    return m_mkt_logger;
}

std::shared_ptr<spdlog::logger> Runner::GetUserLogger() {
    return m_usr_logger;
}

std::shared_ptr<spdlog::logger> Runner::GetStrategyLogger() {
    return m_strategy_logger;
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

void Runner::OnOurTradeAsync(const LimitOrder& order, int executed_qty) {
    // Always notify
    assert(IsReady() && "Connectors should be ready before order processing");
    m_strategy->OnOurTradeAsync(order, executed_qty);
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
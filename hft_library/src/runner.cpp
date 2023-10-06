#include <runner.h>
#include <constants.h>

Runner::Runner(const ConfigType& config, const StrategyGetter& strategy_getter)
        :
        m_config(config),
        m_client(ENDPOINT, config["runner"]["token"].as<std::string>()),
        // TODO: Get/Check instrument information in RunTime
        m_instrument(
                config["runner"]["figi"].as<std::string>(),
                config["runner"]["lot_size"].as<int>(),
                config["runner"]["px_step"].as<double>()
        ),
        m_mkt(*this, config, m_client, m_instrument),
        m_usr(*this, config, m_client, m_instrument),
        m_strategy(strategy_getter(*this)) {
    // Initialize strategy for connectors after connectors and strategy initialization
    m_mkt.m_strategy = m_strategy;
    m_usr.m_strategy = m_strategy;
}

void Runner::Start() {
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

void Runner::OnMarketConnectorReady() {
    m_is_mkt_ready = true;
    std::cout << "MarketConnector is Ready" << std::endl;
    if (IsReady()) {
        m_strategy->OnConnectorsReadiness();
    }
}

void Runner::OnUserConnectorReady() {
    m_is_usr_ready = true;
    std::cout << "UserConnector is Ready" << std::endl;
    if (IsReady()) {
        m_strategy->OnConnectorsReadiness();
    }
}

bool Runner::IsReady() {
    return m_is_mkt_ready & m_is_usr_ready;
}
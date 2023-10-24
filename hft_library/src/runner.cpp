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
        m_strategy(strategy_getter(*this)) {}

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
    std::cout << "UserConnector is Ready" << std::endl;
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
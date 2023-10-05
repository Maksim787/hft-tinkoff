#include <runner.h>
#include <constants.h>

Runner::Runner(const ConfigType& config)
        :
        m_client(ENDPOINT, config["runner"]["token"].as<std::string>()),
        m_mkt(*this, config, m_client),
        m_usr(*this, config, m_client) {

}

void Runner::Start() {
    m_mkt.Start();
    m_usr.Start();
}

void Runner::OnMarketConnectorReady() {
    std::cout << "MarketConnector is Ready" << std::endl;
}

void Runner::OnUserConnectorReady() {
    std::cout << "UserConnector is Ready" << std::endl;
}

bool Runner::IsReady() {
    return m_is_mkt_ready & m_is_usr_ready;
}

#include <connector/user.h>
#include <runner.h>

#include <iostream>

UserConnector::UserConnector(Runner& runner, const ConfigType& config, InvestApiClient& client)
        :
        m_runner(runner),
        m_client(client),
        m_figi(config["runner"]["figi"].as<std::string>()) {}

void UserConnector::Start() {
    std::cout << "Start UserConnector" << std::endl;
    m_orders_stream = std::dynamic_pointer_cast<OrdersStream>(m_client.service("ordersstream"));
    m_orders_stream->TradesStreamAsync(
            {m_figi},
            [this](ServiceReply reply) { OrderStreamCallback(std::move(reply)); }
    );
}

void UserConnector::OrderStreamCallback(ServiceReply reply) {
    if (!m_is_order_stream_ready) {
        m_is_order_stream_ready = true;
        if (this->IsReady()) m_runner.OnUserConnectorReady();
    }
    std::cout << reply.ptr()->DebugString() << std::endl;
}

bool UserConnector::IsReady() {
    return m_is_order_stream_ready;
}

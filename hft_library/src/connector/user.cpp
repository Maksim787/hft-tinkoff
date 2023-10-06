#include <connector/user.h>
#include <connector/utils.h>
#include <runner.h>

#include <iostream>

UserConnector::UserConnector(Runner& runner, const ConfigType& config, InvestApiClient& client, const Instrument& instrument)
        :
        m_runner(runner),
        m_client(client),
        m_instrument(instrument) {}

void UserConnector::Start() {
    std::cout << "Start UserConnector" << std::endl;
    m_orders_stream = std::dynamic_pointer_cast<OrdersStream>(m_client.service("ordersstream"));
    m_orders_stream->TradesStreamAsync(
            {m_instrument.figi},
            [this](ServiceReply reply) { OrderStreamCallback(std::move(reply)); }
    );
}

void UserConnector::OrderStreamCallback(ServiceReply reply) {
    CheckReply(reply);
    if (!m_is_order_stream_ready) {
        m_is_order_stream_ready = true;
        if (this->IsReady()) m_runner.OnUserConnectorReady();
    }
//    std::cout << reply.ptr()->DebugString() << std::endl;
}

bool UserConnector::IsReady() {
    return m_is_order_stream_ready;
}

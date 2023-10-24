#include <connector/user.h>
#include <runner.h>

#include <operationsservice.h>

#include <iostream>


void LimitOrder::Print() const {
    std::cout << "Order " <<
              order_id << ": " <<
              direction << " [" <<
              std::setw(NUMBER_OF_SPACES_PER_NUMBER) << qty <<
              std::setw(NUMBER_OF_SPACES_PER_NUMBER) << px << "]\n";
}

void Positions::Print() const {
    std::cout << "Qty: " << qty << "\n";
    std::cout << "Money: " << money << "\n";
    std::cout << "Orders: " << orders.size() << "\n";
    int i = 0;
    for (const auto&[order_id, order]: orders) {
        std::cout << i << ". ";
        order.Print();
        ++i;
    }
}

UserConnector::UserConnector(Runner& runner, const ConfigType& config, InvestApiClient& client, const Instrument& instrument)
        :
        m_runner(runner),
        m_client(client),
        m_account_id(config["user"]["account_id"].as<std::string>()),
        m_instrument(instrument) {}

const Positions& UserConnector::GetPositions() const {
    return m_positions;
}

void UserConnector::Start() {
    std::cout << "Start UserConnector" << std::endl;

    // Get Initial Positions
    std::cout << "Get Positions" << std::endl;
    auto operations = std::dynamic_pointer_cast<Operations>(m_client.service("operations"));
    auto positions_reply = (operations->GetPositions(m_account_id));
    CheckReply(positions_reply);
    auto positions = dynamic_pointer_cast<PositionsResponse>(positions_reply.ptr());
    assert(positions);

    // Parse Money positions
    const auto& money_positions = positions->money();
    assert(money_positions.size() == 1 && "Found multiple currency positions");
    m_positions.money = MoneyValueToDouble(money_positions[0]);

    // Parse Money blocked positions
    // TODO: add cancel orders
    assert(positions->blocked().empty() && "Cancel Buy orders!");

    // Parse Securities positions
    const auto& securities_positions = positions->securities();
    assert(securities_positions.size() <= 1 && "Found multiple securities positions");
    if (securities_positions.size() == 1) {
        const PositionsSecurities& security_position = securities_positions[0];
        assert(security_position.figi() == m_instrument.figi);
        assert(security_position.blocked() == 0 && "Cancel Sell orders!");
        m_positions.qty = static_cast<int>(security_position.balance());
    }

    // Subscribe OrderStream
    m_orders_stream = std::dynamic_pointer_cast<OrdersStream>(m_client.service("ordersstream"));
    m_orders_stream->TradesStreamAsync(
            {m_instrument.figi},
            [this](ServiceReply reply) { OrderStreamCallback(std::move(reply)); }
    );
    // TODO: check that stream is open
    m_is_order_stream_ready = true;
    m_runner.OnUserConnectorReady();
}

void UserConnector::OrderStreamCallback(ServiceReply reply) {
    CheckReply(reply);
    auto response = dynamic_pointer_cast<TradesStreamResponse>(reply.ptr());
    assert(response);
    if (response->has_order_trades()) {
        // Process our trades
        const OrderTrades& order_trades = response->order_trades();
        assert(order_trades.figi() == m_instrument.figi && "Got unexpected trade for different figi");
        assert(order_trades.account_id() == m_account_id && "Got unexpected trade for different account");

        // TODO: parse time
        const std::string order_id = order_trades.order_id();

        // Buy: 1
        // Sell: 2
        const int direction = order_trades.direction();
        assert(direction == 1 || direction == 2);

        // Calculate total executed qty
        const google::protobuf::RepeatedPtrField<OrderTrade>& trades = order_trades.trades();
        assert(!trades.empty());
        double px = QuotationToDouble(trades[0].price());
        int total_qty = 0;
        for (const OrderTrade& trade: trades) {
            // TODO: parse time
            assert(QuotationToDouble(trade.price()) == px);
            assert(trade.quantity() % m_instrument.lot_size == 0);
            total_qty += m_instrument.QtyToLots(trade.quantity()); // convert to lots
        }
        ProcessOurTrade(order_id, total_qty, m_instrument.DoublePxToInt(px), direction == 1 ? Direction::Buy : Direction::Sell);
    } else {
        // Process ping
        assert(response->has_ping());
    }
}

void UserConnector::ProcessOurTrade(const std::string& order_id, int total_qty, int px, Direction direction) {
    // Find order
    auto it = m_positions.orders.find(order_id);
    assert(it != m_positions.orders.end() && "Unknown order_id");
    LimitOrder& order = it->second;
    // Do sanity check
    assert(order.px == px && "Px mismatch");
    assert(order.direction == direction && "Direction mismatch");
    assert(order.qty <= total_qty && "More qty was executed than order contains");
    // Remove qty
    order.qty -= total_qty;
    // Notify strategy
    m_runner.OnOurTrade(order, total_qty);
    if (order.qty == 0) {
        // Remove empty order after strategy notification
        m_positions.orders.erase(it);
    }
}

bool UserConnector::IsReady() const {
    // TODO: remove
    return m_is_order_stream_ready;
}

#include <iostream>

#include <operationsservice.h>

#include <connector/user.h>
#include <runner.h>


std::ostream& operator<<(std::ostream& os, const LimitOrder& order) {
    os << "Order "
       << order.order_id << ": "
       << order.direction << " ["
       << std::setw(NUMBER_OF_SPACES_PER_NUMBER) << order.qty
       << std::setw(NUMBER_OF_SPACES_PER_NUMBER) << order.px << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Positions& positions) {
    os << "Positions:\n";
    os << "Qty: " << positions.qty << "\n";
    os << "Money: " << positions.money << "\n";
    os << "Orders: " << positions.orders.size() << "\n";
    int i = 0;
    for (const auto&[order_id, order]: positions.orders) {
        os << i << ". ";
        os << order;
        os << "\n";
        ++i;
    }
    return os;
}

UserConnector::UserConnector(Runner& runner, const ConfigType& config)
        :
        m_runner(runner),
        m_client(runner.GetClient()),
        m_logger(runner.GetUserLogger()),
        m_account_id(config["user"]["account_id"].as<std::string>()),
        m_instrument(runner.GetInstrument()) {}

const Positions& UserConnector::GetPositions() const {
    return m_positions;
}

void UserConnector::Start() {
    m_logger->info("Start UserConnector");

    // Get Initial Positions
    m_logger->info("Get Positions and Subscribe Streams");
    auto operations = std::dynamic_pointer_cast<Operations>(m_client.service("operations"));
    ServiceReply positions_reply = operations->GetPositions(m_account_id);
    auto positions = ParseReply<PositionsResponse>(positions_reply, m_logger);

    // Parse Money positions
    const auto& money_positions = positions->money();
    assert(money_positions.size() == 1 && "Found multiple currency positions");
    m_positions.money = m_instrument.MoneyValueToPx(money_positions[0]);

    // Parse Money blocked positions
    // TODO: add cancel orders
    if (!positions->blocked().empty()) {
        m_logger->error("Blocked money positions ({}): ", positions->blocked().size());
        for (const MoneyValue& blocked_positions: positions->blocked()) {
            m_logger->error("currency={} MoneyValue={}.{}", blocked_positions.currency(), blocked_positions.units(), blocked_positions.nano());
        }
        assert(false && "Cancel Buy orders!");
    }

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
            {m_account_id},
            [this](ServiceReply reply) { OrderStreamCallback(ParseReply<TradesStreamResponse>(reply, m_logger)); }
    );

    // Create orders service
    m_orders_service = std::dynamic_pointer_cast<Orders>(m_client.service("orders"));

    // TODO: check that stream is open
    m_is_order_stream_ready = true;
    m_runner.OnUserConnectorReady();
}

const LimitOrder& UserConnector::PostOrder(int px, int qty, Direction direction) {
    // Convert px to Tinkoff API px
    auto[units, nano] = m_instrument.PxToQuotation(px);
    // Send request
    m_logger->info("PostOrder: {} qty={}, px={}.{} ({})", direction, qty, units, nano, px);
    ServiceReply reply = m_orders_service->PostOrder(
            m_instrument.figi,
            qty,
            units,
            nano,
            direction == Direction::Buy ? OrderDirection::ORDER_DIRECTION_BUY : OrderDirection::ORDER_DIRECTION_SELL,
            m_account_id,
            OrderType::ORDER_TYPE_LIMIT,  // only limit orders are supported
            ""  // empty idempotency key
    );
    auto response = ParseReply<PostOrderResponse>(reply, m_logger);
    m_logger->info("PostOrder success");

    // Do sanity check for response
    assert(response->lots_requested() == qty);
    assert(m_instrument.MoneyValueToPx(response->initial_security_price()) == px);
    assert(response->direction() == OrderDirection::ORDER_DIRECTION_BUY && direction == Direction::Buy ||
           response->direction() == OrderDirection::ORDER_DIRECTION_SELL && direction == Direction::Sell);
    assert(response->order_type() == OrderType::ORDER_TYPE_LIMIT);
    assert(response->figi() == m_instrument.figi);

    const std::string& order_id = response->order_id();
    OrderExecutionReportStatus status = response->execution_report_status();
    if (status == OrderExecutionReportStatus::EXECUTION_REPORT_STATUS_NEW) {
        return ProcessNewPostOrder(order_id, px, qty, direction);
    } else if (status == OrderExecutionReportStatus::EXECUTION_REPORT_STATUS_PARTIALLYFILL) {
        // TODO: implement market execution
        assert(false && "Not implemented");
    } else if (status == OrderExecutionReportStatus::EXECUTION_REPORT_STATUS_REJECTED) {
        // TODO: process errors
        assert(false && "Not implemented");
    } else {
        assert(false && "Unreachable");
    }
}

void UserConnector::CancelOrder(const std::string& order_id) {
    // Check order existence
    auto it = m_positions.orders.find(order_id);
    assert(it != m_positions.orders.end());
    // Send request
    m_logger->info("[UserConnector] CancelOrder order_id={}", order_id);
    ServiceReply reply = m_orders_service->CancelOrder(
            m_account_id,
            order_id
    );
    // Remove the order anyway
    m_positions.orders.erase(it);

    auto response = ParseReply<CancelOrderResponse>(reply, m_logger);
    m_logger->info("[UserConnector] CancelOrder success");
    // TODO: parse response->time()
}

void UserConnector::OrderStreamCallback(TradesStreamResponse* response) {
    if (response->has_order_trades()) {
        // Process our trades
        const OrderTrades& order_trades = response->order_trades();
        assert(order_trades.figi() == m_instrument.figi && "Got unexpected trade for different figi");
        assert(order_trades.account_id() == m_account_id && "Got unexpected trade for different account");

        // TODO: parse time
        const std::string& order_id = order_trades.order_id();

        const int direction = order_trades.direction();
        assert(direction == OrderDirection::ORDER_DIRECTION_BUY || direction == OrderDirection::ORDER_DIRECTION_SELL);

        // Calculate total executed qty
        const google::protobuf::RepeatedPtrField<OrderTrade>& trades = order_trades.trades();
        assert(!trades.empty());
        const int px = m_instrument.QuotationToPx(trades[0].price());
        int executed_qty = 0;
        for (const OrderTrade& trade: trades) {
            // TODO: parse time and trade_id
            assert(m_instrument.QuotationToPx(trade.price()) == px);
            assert(trade.quantity() % m_instrument.lot_size == 0);
            executed_qty += m_instrument.QtyToLots(trade.quantity()); // convert to lots
        }
        ProcessOurTrade(order_id, px, executed_qty, direction == OrderDirection::ORDER_DIRECTION_BUY ? Direction::Buy : Direction::Sell);
    } else {
        // Process ping
        assert(response->has_ping());
    }
}

void UserConnector::ProcessOurTrade(const std::string& order_id, int px, int executed_qty, Direction direction) {
    m_logger->info("OurTrade: {} order_id={}, qty={}, px={}", direction, order_id, executed_qty, px);
    // Find order
    auto it = m_positions.orders.find(order_id);
    bool order_exists = (it != m_positions.orders.end());
    if (!order_exists) {
        m_logger->info("Execution of the cancelled order");
        // TODO: add storage with cancelled and executed orders
    } else {
        LimitOrder& order = it->second;
        // Do sanity check
        assert(order.px == px && "Px mismatch");
        assert(order.direction == direction && "Direction mismatch");
        assert(executed_qty <= order.qty && "More qty was executed than order contains");
        // Remove qty
        order.qty -= executed_qty;
    }
    // Update positions
    int signed_qty = executed_qty * (direction == Direction::Buy ? 1 : -1);
    m_positions.qty += signed_qty;
    m_positions.money -= signed_qty * px;

    // Copy order information
    LimitOrder order = order_exists ? it->second : LimitOrder {
            .order_id = order_id,
            .direction = direction,
            .px = px,
            .qty = executed_qty
    };

    // Remove empty order before strategy notification
    if (it != m_positions.orders.end() && it->second.qty == 0) {
        m_positions.orders.erase(it);
    }

    // Notify strategy
    m_runner.OnOurTrade(order, executed_qty);
}

const LimitOrder& UserConnector::ProcessNewPostOrder(const std::string& order_id, int px, int qty, Direction direction) {
    assert(!m_positions.orders.contains(order_id));
    // Add order to current orders
    auto it = m_positions.orders.emplace(
            order_id,
            LimitOrder {
                    .order_id = order_id,
                    .direction = direction,
                    .px = px,
                    .qty = qty
            }
    );
    const LimitOrder& new_order = it.first->second;
    m_logger->info("New order is placed: {}", new_order);
    return new_order;
}

bool UserConnector::IsReady() const {
    // TODO: remove
    return m_is_order_stream_ready;
}
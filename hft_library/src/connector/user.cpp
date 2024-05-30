#include "connector/user.h"

#include <iomanip>
#include <iostream>

#include "hft_library/third_party/TinkoffInvestSDK/services/operationsservice.h"
#include "runner.h"

std::ostream& operator<<(std::ostream& os, const LimitOrder& order) {
    os << "Order "
       << order.order_id << ": "
       << order.direction << " ["
       << std::setw(NUMBER_OF_SPACES_PER_NUMBER) << order.qty
       << std::setw(NUMBER_OF_SPACES_PER_NUMBER) << order.px << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, const Positions& positions) {
    os << "Qty: " << positions.qty << "\n";
    os << "Money: " << positions.money << "\n";
    os << "Orders: " << positions.orders.size() << "\n";
    int i = 0;
    for (const auto& [order_id, order] : positions.orders) {
        os << i << ". ";
        os << order;
        os << "\n";
        ++i;
    }
    return os;
}

UserConnector::UserConnector(Runner& runner, const ConfigType& config)
    : m_runner(runner),
      m_client(runner.GetClient()),
      m_logger(runner.GetLogger("runner", false)),
      m_our_trades_logger(runner.GetLogger("our_trades", true)),
      m_positions_logger(runner.GetLogger("positions", true)),
      m_orders_logger(runner.GetLogger("orders", true)),
      m_account_id(config["user"]["account_id"].as<std::string>()),
      m_instrument(runner.GetInstrument()) {
    m_our_trades_logger->info("internal_log_id,strategy_time,direction,order_id,executed_qty,px");
    m_positions_logger->info("internal_log_id,strategy_time,qty,money");
    m_orders_logger->info("internal_log_id,strategy_time,order_id,direction,px,qty");
}

const Positions& UserConnector::GetPositions() const {
    return m_positions;
}

void UserConnector::Start() {
    m_logger->info("Start UserConnector");

    // Get Initial Positions
    m_logger->info("Get Positions");
    auto operations = std::dynamic_pointer_cast<Operations>(m_client.service("operations"));
    ServiceReply positions_reply = operations->GetPositions(m_account_id);
    auto positions = ParseReply<PositionsResponse>(positions_reply, m_logger);

    // Parse Money positions
    const auto& money_positions = positions->money();
    assert(money_positions.size() <= 1 && "Found multiple currency positions");
    if (money_positions.empty()) {
        m_positions.money = 0;
    } else {
        m_positions.money = m_instrument.MoneyValueToPx(money_positions[0]);
        assert(m_positions.money >= 0);
    }

    // Parse Money blocked positions
    // TODO: add cancel orders
    if (!positions->blocked().empty()) {
        m_logger->error("Blocked money positions ({}): ", positions->blocked().size());
        for (const MoneyValue& blocked_positions : positions->blocked()) {
            m_logger->error("currency={} MoneyValue={}.{}", blocked_positions.currency(), blocked_positions.units(), blocked_positions.nano());
        }
        assert(false && "Cancel Buy orders!");
    }

    // Parse Securities positions
    const auto& securities_positions = positions->securities();
    for (const PositionsSecurities& security_position : securities_positions) {
        assert(security_position.blocked() == 0 && "Cancel Sell orders!");
        if (security_position.figi() == m_instrument.figi) {
            m_positions.qty = static_cast<int>(security_position.balance());
        }
    }

    m_logger->info("Subscribe OrderStream");
    // Subscribe OrderStream
    m_orders_stream = std::dynamic_pointer_cast<OrdersStream>(m_client.service("ordersstream"));
    m_orders_stream->TradesStreamAsync(
        {m_account_id},
        [this](ServiceReply reply) { OrderStreamCallback(ParseReply<TradesStreamResponse>(reply, m_logger)); });

    // Create orders service
    m_orders_service = std::dynamic_pointer_cast<Orders>(m_client.service("orders"));

    // TODO: check that stream is open
    m_is_order_stream_ready = true;
    m_runner.OnUserConnectorReady();
}

const LimitOrder& UserConnector::PostOrder(int px, int qty, Direction direction) {
    // Convert px to Tinkoff API px
    auto [units, nano] = m_instrument.PxToQuotation(px);
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
        ""                            // empty idempotency key
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
        // TODO: implement market execution (it seems that this branch never happens)
        assert(false && "Not implemented");
    } else if (status == OrderExecutionReportStatus::EXECUTION_REPORT_STATUS_REJECTED) {
        // TODO: process errors (it seems that this branch never happens)
        assert(false && "Not implemented");
    } else {
        assert(false && "Unreachable");
    }
}

// TODO: CancelAll()

void UserConnector::CancelOrder(const std::string& order_id) {
    // Check order existence
    auto it = m_positions.orders.find(order_id);
    assert(it != m_positions.orders.end());
    // Send request
    m_logger->info("CancelOrder order_id={} {} qty={}, px={}", order_id, it->second.direction, it->second.qty, it->second.px * m_instrument.px_step);
    ServiceReply reply = m_orders_service->CancelOrder(
        m_account_id,
        order_id);
    // Check for errors
    auto response = ParseReply<CancelOrderResponse>(reply, m_logger);

    // Remove the order if no errors occured
    m_positions.orders.erase(it);

    // TODO: parse response->time()
    // Log Orders
    LogOrders();
    m_logger->info("CancelOrder success");
}

void UserConnector::OrderStreamCallback(TradesStreamResponse* response) {
    if (response->has_order_trades()) {
        LockGuard lock = m_runner.GetEventLock();
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
        for (const OrderTrade& trade : trades) {
            // TODO: parse time and trade_id
            assert(m_instrument.QuotationToPx(trade.price()) == px);
            assert(trade.quantity() % m_instrument.lot_size == 0);
            executed_qty += m_instrument.QtyToLots(trade.quantity());  // convert to lots
        }

        ProcessOurTrade(lock, order_id, px, executed_qty, direction == OrderDirection::ORDER_DIRECTION_BUY ? Direction::Buy : Direction::Sell);
    } else {
        // Process ping
        assert(response->has_ping());
    }
}

void UserConnector::ProcessOurTrade(const LockGuard& lock, const std::string& order_id, int px, int executed_qty, Direction direction) {
    // Log OurTrade
    TimeType t = current_time();
    m_our_trades_logger->info("{},{},{},{},{},{}", internal_log_id, t, direction, order_id, executed_qty, px);
    m_logger->info("OurTrade: {} order_id={}, qty={}, px={}", direction, order_id, executed_qty, px);
    // Find order
    auto it = m_positions.orders.find(order_id);
    bool order_exists = (it != m_positions.orders.end());
    if (!order_exists) {
        m_logger->error("Execution of the cancelled order: {}", order_id);
        // TODO: add storage with cancelled and executed orders
    } else {
        LimitOrder& order = it->second;
        // Do sanity check
        if (order.px != px) {
            m_logger->error("Order: {}. Price mismatch: px = {}", order, px);
        }
        // assert(order.px == px && "Px mismatch");
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
    LimitOrder order = order_exists ? it->second : LimitOrder{.order_id = order_id, .direction = direction, .px = px, .qty = 0};

    // Remove empty order before strategy notification
    if (it != m_positions.orders.end() && it->second.qty == 0) {
        m_positions.orders.erase(it);
    }

    // Log positions after update
    m_positions_logger->info("{},{},{},{}", internal_log_id, current_time(), m_positions.qty, m_positions.money);
    // Log Orders
    LogOrders();

    // Notify strategy (lock all other events)
    m_runner.OnOurTrade(order, executed_qty);
}

const LimitOrder& UserConnector::ProcessNewPostOrder(const std::string& order_id, int px, int qty, Direction direction) {
    assert(!m_positions.orders.contains(order_id));
    // Add order to current orders
    auto it = m_positions.orders.emplace(
        order_id,
        LimitOrder{
            .order_id = order_id,
            .direction = direction,
            .px = px,
            .qty = qty});
    const LimitOrder& new_order = it.first->second;
    // Log Orders
    LogOrders();
    m_logger->info("New order is placed: {}", new_order);
    return new_order;
}

bool UserConnector::IsReady() const {
    // TODO: remove
    return m_is_order_stream_ready;
}

void UserConnector::LogOrders() {
    for (const auto& [order_id, limit_order] : m_positions.orders) {
        assert(order_id == limit_order.order_id);
        m_orders_logger->info("{},{},{},{},{},{}", internal_log_id, current_time(), order_id, limit_order.direction, limit_order.px, limit_order.qty);
    }
    ++internal_log_id; // increment internal log id
}

#include "strategy.h"
#include "runner.h"

Strategy::Strategy(Runner& runner)
        :
        m_runner(runner),
        m_logger(runner.GetLogger("strategy", false)),
        m_config(runner.GetConfig()),
        m_instrument(runner.GetInstrument()),
        m_order_book(runner.GetMarketConnector().GetOrderBook()),
        m_trades(runner.GetMarketConnector().GetTrades()),
        m_positions(runner.GetUserConnector().GetPositions()) {}

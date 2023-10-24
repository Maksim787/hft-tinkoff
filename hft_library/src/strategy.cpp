#include <strategy.h>
#include <runner.h>

Strategy::Strategy(Runner& runner)
        :
        m_runner(runner),
        m_mkt(runner.GetMarketConnector()),
        m_usr(runner.GetUserConnector()),
        m_config(runner.GetConfig()),
        m_instrument(runner.GetInstrument()),
        m_order_book(m_mkt.GetOrderBook()),
        m_trades(m_mkt.GetTrades()),
        m_positions(m_usr.GetPositions()) {}

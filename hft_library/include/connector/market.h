#pragma once

#include <constants.h>
#include <connector/utils.h>

#include <investapiclient.h>
#include <marketdatastreamservice.h>

class Runner;

class MarketConnector;

class OneSideMarketOrderBook {
public:
    // 1.
    // bid_px < 0
    // ask_px > 0
    // 2.
    // A < B => A is closer to mid-price
    // 3.
    // bid[0], ask[0] => best bid/ask
    // 4.
    // px = real_px / px_step
    // qty = real_qty / lot_size
    int px[MAX_DEPTH];
    int qty[MAX_DEPTH];
};

class MarketOrderBook {
public:
    int depth;

    OneSideMarketOrderBook bid;
    OneSideMarketOrderBook ask;

private:
    const InstrumentInfo& m_instrument;

public:
    MarketOrderBook(const InstrumentInfo& instrument, int depth);

private:
    friend class MarketConnector;

    void Update(bool is_bid, double* px, int* qty);
};

struct MarketTrade {
    Direction direction;
    int px; // real_px / px_step
    int qty; // in lots
};

class Trades {
public:
    // TODO: add last n trades tracking
    bool has_trade = false;
    MarketTrade last_trade = {Direction::Buy, 0, 0};

private:
    const InstrumentInfo& m_instrument;

public:
    Trades(const InstrumentInfo& instrument);

    void Update(Direction direction, double px, int qty);
};

class MarketConnector {
private:
    // Runner
    Runner& m_runner;
    // Client
    InvestApiClient& m_client;
    // Instrument
    const InstrumentInfo& m_instrument;
    // MarketDataStream
    std::shared_ptr<MarketDataStream> m_market_data_stream;
    // Readiness
    bool m_is_order_book_stream_ready = false;
    bool m_is_trade_stream_ready = false;
    // OrderBook
    MarketOrderBook m_order_book;
    // Trades
    Trades m_trades;
public:
    MarketConnector(Runner& runner, const ConfigType& config, InvestApiClient& client, const InstrumentInfo& instrument);

private:
    // Methods for Runner
    friend class Runner;

    void Start();

    // Methods for MarketConnector
    void OrderBookStreamCallBack(ServiceReply reply);

    void TradeStreamCallBack(ServiceReply reply);

    bool IsReady();

    void ParseLevels(const google::protobuf::RepeatedPtrField<Order>& orders, double* px, int* qty);
};

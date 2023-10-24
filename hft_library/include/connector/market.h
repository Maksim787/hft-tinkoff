#pragma once

#include <constants.h>
#include <connector/utils.h>

#include <investapiclient.h>
#include <marketdatastreamservice.h>

class Runner;

class MarketConnector;

class MarketOrderBook;

template <bool IsBidParameter>
class OneSideMarketOrderBook {
public:
    // bid[0], ask[0] => best bid/ask
    // px = real_px / px_step
    // qty = real_qty / lot_size
    int px[MAX_DEPTH] = {0};
    int qty[MAX_DEPTH] = {0};
    int depth;

    OneSideMarketOrderBook(int depth);

    constexpr bool IsBid();

private:
    friend MarketOrderBook;
};

class MarketOrderBook {
public:
    OneSideMarketOrderBook<true> bid;
    OneSideMarketOrderBook<false> ask;

    int depth;

private:
    const Instrument& m_instrument;

public:
    MarketOrderBook(const Instrument& instrument, int depth);

    void Print() const;

private:
    friend class MarketConnector;

    template <bool IsBid>
    OneSideMarketOrderBook<IsBid>& GetOneSideOrderBook();

    template <bool IsBid>
    void Update(const double* px, const int* qty);
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
    const Instrument& m_instrument;

public:
    Trades(const Instrument& instrument);

    void Print() const;

private:
    friend class MarketConnector;

    void Update(Direction direction, double px, int qty);
};

class MarketConnector {
private:
    // Runner
    Runner& m_runner;
    // Client
    InvestApiClient& m_client;
    // Instrument
    const Instrument& m_instrument;

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
    MarketConnector(Runner& runner, const ConfigType& config, InvestApiClient& client, const Instrument& instrument);

    // Getters
    const MarketOrderBook& GetOrderBook() const;

    const Trades& GetTrades() const;

private:
    // Methods for Runner
    friend class Runner;

    void Start();

    // Methods for MarketConnector
    void OrderBookStreamCallBack(ServiceReply reply);

    void TradeStreamCallBack(ServiceReply reply);

    [[nodiscard]] bool IsReady() const;

    void ParseLevels(const google::protobuf::RepeatedPtrField<Order>& orders, double* px, int* qty) const;
};

#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include "hft_library/third_party/TinkoffInvestSDK/investapiclient.h"
#include "hft_library/third_party/TinkoffInvestSDK/services/marketdatastreamservice.h"

#include "constants.h"
#include "connector/utils.h"

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

    constexpr static bool IsBid() {
        return IsBidParameter;
    }

    // 1 if IsBid; -1 if IsAsk
    constexpr static int Sign() {
        if constexpr (IsBidParameter) {
            return 1;
        } else {
            return -1;
        }
    }

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

    template <bool IsBid>
    OneSideMarketOrderBook<IsBid>& GetOneSideOrderBook();

private:
    friend class MarketConnector;

    template <bool IsBid>
    void Update(const int* px, const int* qty);
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

private:
    friend class MarketConnector;

    void Update(Direction direction, int px, int qty);
};

std::ostream& operator<<(std::ostream& os, const Trades& trades);

std::ostream& operator<<(std::ostream& os, const MarketOrderBook& ob);


class MarketConnector {
private:
    // Runner
    Runner& m_runner;
    // Client
    InvestApiClient& m_client;
    // Logger
    std::shared_ptr<spdlog::logger> m_logger;
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
    MarketConnector(Runner& runner, const ConfigType& config);

    // Getters
    const MarketOrderBook& GetOrderBook() const;

    const Trades& GetTrades() const;

private:
    // Methods for Runner
    friend class Runner;

    void Start();

    // Methods for MarketConnector
    void OrderBookStreamCallBack(MarketDataResponse* response);

    void TradeStreamCallBack(MarketDataResponse* response);

    [[nodiscard]] bool IsReady() const;

    void ParseLevels(const google::protobuf::RepeatedPtrField<Order>& orders, int* px, int* qty) const;
};

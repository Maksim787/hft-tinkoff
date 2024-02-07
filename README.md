# HFT with Tinkoff Invest API

## HFT library

### Executables

`hft_library/` — infrastructure and strategies written in C++.

`hft_library/exe/` — directory with executables:

1. grid_trading.cpp — main strategy
2. market_making.cpp — old strategy (legacy)
3. test_yaml.cpp — test config reader
4. test_tinkoff.cpp — test Tinkoff API functions

### Library implementation

1. Runner — launch connectors and strategy
2. MarketConnector — receive orderbooks and trades
3. UserConnector — receive our trades and post/cancel orders
4. Strategy — interact with connectors and runner

Notes on implementation:

1. Always notify strategy on our trade in `OnOurTradeAsync` callback.
2. Do not notify about events if more events are pending. For instance, we got simultaneously two updates from exchange. The strategy will be notified only about the last one.
3. Post and Cancel orders block strategy for some time. It may be good to check that more events are pending and to stop posting orders.

## Python scripts

`scripts/` — python helper scripts.

`commmon/utils.py` — common functions (read config, convert quotation to float, convert timezones).

`research/load_trades.py` — download operations via `GetOperationsByCursor` and positions via `GetPositions` from Tinkoff API for data analysis.

`strategy_utils/cancel_all.py` — cancel all our orders.

`strategy_utils/find_figi.py` — find figi (tinkoff instrument id) of the instrument by ticker.

`telegram-bot/bot.py` — monitor our positions and orders and send messages to telegram channel.

`telegram-bot/test_bot.py` — send messages to bot from strategy. It is used to test bot.

Some scripts that are not used now:

`strategy_utils/find_class_codes.py` — get the possible class codes from Tinkoff API. To get idea what the class code is.

`strategy_utils/find_instruments_to_trade.py` — find the cheapest instruments (price of one lot of the instrument) that are not already bought on the account.

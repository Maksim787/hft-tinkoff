import sys
import os

sys.path.append(os.getcwd())

from scripts.common.utils import read_config, quotation_to_float, to_moscow

import tinkoff.invest as inv
import pandas as pd
import logging
import asyncio
import datetime
import traceback
from dataclasses import dataclass
from collections import deque
from aiogram import Bot, Dispatcher, types


config = read_config()
dp = Dispatcher()
bot = Bot(config["telegram"]["token"])
channel_id = None


class RateLimiter:
    EPS = 1e-3

    def __init__(self, bot_limit_messages: float, bot_limit_interval_seconds: float) -> None:
        self.bot_limit_messages = bot_limit_messages
        self.bot_limit_interval_seconds = bot_limit_interval_seconds
        self.queue: deque[datetime.datetime] = deque()

    async def wait(self):
        self.pop_requests_from_queue()
        if len(self.queue) >= self.bot_limit_messages:
            now = datetime.datetime.now()
            sleep_time = self.queue[0] - (now - datetime.timedelta(seconds=self.bot_limit_interval_seconds))
            sleep_time = sleep_time.total_seconds() + self.EPS
            print(f"Sleep {sleep_time} seconds: now={now}; queue={self.queue}")
            await asyncio.sleep(sleep_time)
            self.pop_requests_from_queue()
        self.queue.append(datetime.datetime.now())

    def pop_requests_from_queue(self):
        now = datetime.datetime.now()
        while self.queue and (now - self.queue[0]).total_seconds() > self.bot_limit_interval_seconds:
            self.queue.popleft()


limiter = RateLimiter(config["telegram"]["bot_limit_messages"], config["telegram"]["bot_limit_interval_seconds"])


########################################################
# Send message
########################################################
async def send_message(message: str):
    print(f"Send: {message}")
    # TODO: rate limit
    try:
        await limiter.wait()
        await bot.send_message(channel_id, message, parse_mode="MarkdownV2")
        print("Sent")
    # except aiogram.exceptions.TelegramRetryAfter as ex:
    # except aiogram.exceptions.TelegramNetworkError as ex:
    except Exception as ex:
        exception_message = f"Exception:\n{ex}\nTraceback:\n{traceback.format_exc()}"
        print(exception_message)
        await asyncio.sleep(1)
        await bot.send_message(channel_id, f"```\n{exception_message}\n```", parse_mode="MarkdownV2")
        await bot.send_message(channel_id, message, parse_mode="MarkdownV2")
        print("Not sent")


########################################################
# Read Positions Stream from Tinkoff (not used)
########################################################
async def tinkoff_stream_task():
    async with inv.AsyncClient(token=config["runner"]["token"]) as client:
        stream = client.operations_stream.positions_stream(accounts=[str(config["user"]["account_id"])])
        async for position in stream:
            print(f"Got position: {position}")
            if position.subscriptions:
                assert len(position.subscriptions.accounts) == 1
                assert position.subscriptions.accounts[0].subscription_status == inv.PositionsAccountSubscriptionStatus.POSITIONS_SUBSCRIPTION_STATUS_SUCCESS
                await send_message("Start positions subscription")
                print("Start bot")
                continue
            position = position.position
            assert position is not None

            columns = ["asset", "total", "blocked", "available"]
            df = []
            for money in position.money:
                currency = money.available_value.currency
                blocked = quotation_to_float(money.blocked_value)
                available = quotation_to_float(money.available_value)
                total = blocked + available
                df.append([currency, total, blocked, available])
            for security in position.securities:
                name = security.figi
                blocked = security.blocked
                available = security.balance
                total = blocked + available
                df.append([name, total, blocked, available])
            df = pd.DataFrame(df, columns=columns)
            df.index.name = None

            time = to_moscow(position.date).time()
            message = f"```[{time}] Positions:\n" + df.to_string() + "```"
            print(message)
            await send_message(message)


########################################################
# TinkoffTask
########################################################
class TinkoffTask:
    def __init__(self) -> None:
        self.account_id = str(config["user"]["account_id"])
        self.figi = config["runner"]["figi"]
        self.client = None
        self.operations_by_id: dict[str, inv.OperationItem] = {}
        self.old_positions = None
        self.old_orders = {}
        self.all_orders = {}

    ########################################################
    # Read Positions from Tinkoff
    ########################################################
    @dataclass
    class Positions:
        last_price: float

        money_blocked: float
        money_balance: float
        money_total: float

        qty_blocked: int
        qty_balance: int
        qty_total: int

        capital: float

        bid_orders: int
        ask_orders: int

        def __init__(self, positions: inv.PositionsResponse, last_price: inv.MoneyValue):
            assert len(positions.money) <= 1
            assert len(positions.blocked) <= 1
            securities = [s for s in positions.securities if s.figi == config["runner"]["figi"]]
            assert len(securities) <= 1

            self.money_balance = quotation_to_float(positions.money[0]) if positions.money else 0.0
            self.money_blocked = quotation_to_float(positions.blocked[0]) if positions.blocked else 0.0
            if securities:
                security = securities[0]
                self.qty_blocked = security.blocked
                self.qty_balance = security.balance
            else:
                self.qty_blocked = 0
                self.qty_balance = 0
            self.last_price = last_price

            self.money_total = self.money_blocked + self.money_balance
            self.qty_total = self.qty_blocked + self.qty_balance
            self.capital = self.money_total + self.qty_total * self.last_price
            self.ask_orders = self.qty_blocked
            self.bid_orders = round(self.money_blocked / self.last_price)

        def __eq__(self, other) -> bool:
            for field in ["money_blocked", "money_balance", "qty_blocked", "qty_balance", "bid_orders", "ask_orders"]:
                if getattr(self, field) != getattr(other, field):
                    print(f"Diff in {field}: {self}, {other}")
                    return False
            return True

    class Order:
        BUY = 0
        SELL = 1

        PARTIAL_FILL = "Partial fill"
        NEW = "New"

        ORDER_STATUS = {
            inv.OrderExecutionReportStatus.EXECUTION_REPORT_STATUS_PARTIALLYFILL: PARTIAL_FILL,
            inv.OrderExecutionReportStatus.EXECUTION_REPORT_STATUS_NEW: NEW,
        }

        def __init__(self, order: inv.OrderState):
            assert order.order_type == inv.OrderType.ORDER_TYPE_LIMIT

            assert order.figi == config["runner"]["figi"]
            self.id = order.order_id
            self.status = self.ORDER_STATUS[order.execution_report_status]
            self.direction = self.BUY if order.direction == inv.OrderDirection.ORDER_DIRECTION_BUY else self.SELL
            self.qty_requested = order.lots_requested
            self.qty_executed = order.lots_executed
            self.px = quotation_to_float(order.initial_order_price) / self.qty_requested
            self.place_time = to_moscow(order.order_date)

        def __eq__(self, other):
            for field in ["id", "status", "direction", "qty_requested", "qty_executed", "px", "place_time"]:
                if getattr(self, field) != getattr(other, field):
                    print(f"Diff in {field}: {self}, {other}")
                    return False
            return True

    def format_positions(self, old_positions: Positions | None, positions: Positions, old_orders: list[Order], orders: list[Order]):
        # money_blocked: {money_blocked:9.2f}  |  {money_blocked / money_total:5.1%}
        # money_balance: {money_balance:9.2f}  |  {money_balance / money_total:5.1%}

        # qty_blocked:   {qty_blocked:9}  |  {qty_blocked / qty_total:5.1%}
        # qty_balance:   {qty_balance:9}  |  {qty_balance / qty_total:5.1%}
        def get_bid_ask_orders(orders):
            bid_orders = {order_id: order for order_id, order in orders.items() if order.direction == self.Order.BUY}
            ask_orders = {order_id: order for order_id, order in orders.items() if order.direction == self.Order.SELL}
            bid_px = sorted([order.px for order in bid_orders.values()], reverse=True)
            ask_px = sorted([order.px for order in ask_orders.values()])
            return bid_orders, ask_orders, bid_px, ask_px

        bid_orders, ask_orders, bid_px, ask_px = get_bid_ask_orders(orders)

        px = f"{positions.last_price:9.1f}"
        capital = f"{positions.capital:9.2f}"
        money = f"{positions.money_total:9.2f}"
        qty = f"{positions.qty_total:9}"
        len_bid_orders = f"{len(bid_orders):2}"
        len_ask_orders = f"{len(ask_orders):2}"
        bid_px_range = f"[{bid_px[0]} - {bid_px[-1]}]" if bid_px else f"[]"
        ask_px_range = f"[{ask_px[0]} - {ask_px[-1]}]" if ask_px else f"[]"
        if old_positions is not None:
            assert old_orders is not None
            old_bid_orders, old_ask_orders, old_bid_px, old_ask_px = get_bid_ask_orders(old_orders)
            old_px = f"{old_positions.last_price:9.1f}"
            old_capital = f"{old_positions.capital:9.2f}"
            old_money = f"{old_positions.money_total:9.2f}"
            old_qty = f"{old_positions.qty_total:9}"
            old_len_bid_orders = f"{len(old_bid_orders):2}"
            old_len_ask_orders = f"{len(old_ask_orders):2}"
            old_bid_px_range = f"[{old_bid_px[0]} - {old_bid_px[-1]}]" if old_bid_px else f"[]"
            old_ask_px_range = f"[{old_ask_px[0]} - {old_ask_px[-1]}]" if old_ask_px else f"[]"
            if px != old_px:
                px = f"{old_px} -> {px}"
            if money != old_money:
                money = f"{old_money} -> {money}"
            if capital != old_capital:
                change = positions.capital - old_positions.capital
                if change > 0:
                    change = f"+{change}"
                capital = f"{old_capital} -> {capital} [{change}]"
            if qty != old_qty:
                qty = f"{old_qty} -> {qty}"
            if len_bid_orders != old_len_bid_orders:
                len_bid_orders = f"{old_len_bid_orders} -> {len_bid_orders}"
            if len_ask_orders != old_len_ask_orders:
                len_ask_orders = f"{old_len_ask_orders} -> {len_ask_orders}"
            if old_bid_px_range != bid_px_range:
                bid_px_range = f"{old_bid_px_range} -> {bid_px_range}"
            if old_ask_px_range != ask_px_range:
                ask_px_range = f"{old_ask_px_range} -> {ask_px_range}"

        return f"""```Positions
capital: {capital}
px:      {px}
money:   {money}
qty:     {qty}

bid_orders: {len_bid_orders} {bid_px_range}
ask_orders: {len_ask_orders} {ask_px_range}
spread:     {ask_px[0] - bid_px[0] if bid_px and ask_px else None}
```"""

    async def send_positions(self):
        last_price = quotation_to_float((await self.client.market_data.get_last_prices(figi=[self.figi])).last_prices[0].price)

        positions = await self.client.operations.get_positions(account_id=self.account_id)
        positions = self.Positions(positions, last_price)

        orders = (await self.client.orders.get_orders(account_id=self.account_id)).orders
        orders = [self.Order(order) for order in orders]
        orders = {order.id: order for order in orders}
        for order_id, order in orders.items():
            self.all_orders[order_id] = order

        if self.old_positions is None or self.old_positions != positions or orders != self.old_orders:
            await send_message(self.format_positions(self.old_positions, positions, self.old_orders, orders))
            self.old_positions = positions
            self.old_orders = orders

    async def trades_stream(self):
        stream = self.client.orders_stream.trades_stream(accounts=[self.account_id])
        async for trade in stream:
            if trade.order_trades is None:
                continue
            trade = trade.order_trades
            order_id = trade.order_id
            order = self.all_orders.get(order_id)
            direction = "Buy" if trade.direction == inv.OrderDirection.ORDER_DIRECTION_BUY else "Sell"
            assert trade.figi == self.figi
            trade_time = to_moscow(trade.trades[0].date_time)
            px = quotation_to_float(trade.trades[0].price)
            qty = sum(t.quantity for t in trade.trades)

            place_time = order.place_time.time() if order is not None else None
            message = f"""```Trade
{direction}
qty  = {qty}
px   = {px}

trade_time = {trade_time.time()}
place_time = {place_time}
order_id   = {order_id}
```"""
            await send_message(message)

    async def positions_monitor(self):
        tinkoff_api_interval_seconds = config["telegram"]["tinkoff_api_interval_seconds"]
        while True:
            now = datetime.datetime.now()
            await self.send_positions()
            elapsed = (datetime.datetime.now() - now).total_seconds()
            if tinkoff_api_interval_seconds > elapsed:
                await asyncio.sleep(tinkoff_api_interval_seconds - elapsed)

    async def run_task(self):
        async with inv.AsyncClient(token=config["runner"]["token"]) as client:
            self.client = client
            await send_message("Start monitoring positions")
            await self.positions_monitor()
            # await asyncio.gather(self.trades_stream(), self.positions_monitor())


########################################################
# Accept messages from other applications
########################################################
async def messages_task():
    async def handle_client(reader, writer):
        while True:
            message = (await reader.read(4096)).decode("utf8")
            if not message:
                return
            print(f"Got: {message}")
            await send_message(message)
            print("Sent")

    server = await asyncio.start_server(handle_client, "localhost", config["telegram"]["port"])
    async with server:
        await server.serve_forever()


########################################################
# Answer ping messages
########################################################
@dp.message()
async def ping_task(message: types.Message) -> None:
    await message.send_copy(chat_id=message.chat.id)


########################################################
# Run tasks
########################################################
async def main():
    channel_info = await bot.get_chat("@" + (config["telegram"]["channel_id"] if not config["server"]["debug"] else config["telegram"]["debug_channel_id"]))
    global channel_id
    channel_id = channel_info.id

    # await asyncio.gather(messages_task(), tinkoff_task())
    # await asyncio.gather(dp.start_polling(bot), messages_task(), tinkoff_task())
    tinkoff_task = TinkoffTask()
    await asyncio.gather(tinkoff_task.run_task())


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, stream=sys.stdout)
    asyncio.run(main())

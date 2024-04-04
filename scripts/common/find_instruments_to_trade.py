import sys
import os

sys.path.append(os.getcwd())

import tinkoff.invest as inv
from dataclasses import dataclass
from scripts.common.utils import quotation_to_float, parse_config

# Config:
MAIN_ACCOUNT_NAME = 'Портфель'
MAIN_ACCOUNT_NAME = 'Алготрейдинг'


@dataclass
class Position:
    figi: str
    ticker: str
    class_code: str
    name: str
    balance: int  # not in lots
    min_price_increment: float
    lot: int
    price: float


@dataclass
class PossiblePosition:
    figi: str
    ticker: str
    class_code: str
    name: str
    min_price_increment: float
    lot: int
    price: float


def filter_condition(share: inv.Share):
    if share.ticker in ['TCSG', 'TMOS']:
        return True
    return share.country_of_risk == 'RU' and share.currency == 'rub' and not share.for_qual_investor_flag and not share.otc_flag and share.short_enabled_flag and share.buy_available_flag and share.sell_available_flag


def main():
    config = parse_config()
    with inv.Client(token=config.token) as client:
        shares = client.instruments.shares().instruments
        print('Get shares')
        shares = {share.figi: share for share in shares if filter_condition(share)}

        print('Get last prices')
        last_prices = client.market_data.get_last_prices(figi=[share.figi for share in shares.values()]).last_prices
        last_prices = {last_price.figi: quotation_to_float(last_price.price) for last_price in last_prices}

        print('Get accounts')
        accounts = client.users.get_accounts().accounts
        for account in accounts:
            print(f'{account.name}: {account.id}: {account}')
        try:
            main_account = next(filter(lambda x: x.name == MAIN_ACCOUNT_NAME, accounts))
        except StopIteration:
            print([a.name for a in accounts])
            raise
        account_id = main_account.id

        positions = client.operations.get_positions(account_id=account_id).securities
        clean_positions = []
        for position in positions:
            if position.instrument_type != 'share':
                continue
            if position.figi not in shares:
                continue
            share = shares[position.figi]
            clean_positions.append(
                Position(
                    figi=position.figi,
                    class_code=share.class_code,
                    ticker=share.ticker,
                    name=share.name,
                    balance=position.balance,
                    min_price_increment=quotation_to_float(share.min_price_increment),
                    lot=share.lot,
                    price=last_prices[position.figi]
                )
            )
        figi_in_portfolio = {pos.figi for pos in clean_positions}

    clean_positions.sort(key=lambda pos: pos.balance * pos.price)
    for pos in clean_positions:
        print(f'min_capital={pos.lot * pos.price}, curr_pos={pos.balance * pos.price}', pos)
    print('=' * 50)

    possible_positions = []
    for figi, share in shares.items():
        if figi not in figi_in_portfolio:
            possible_positions.append(
                PossiblePosition(
                    figi=figi,
                    ticker=share.ticker,
                    class_code=share.class_code,
                    name=share.name,
                    min_price_increment=quotation_to_float(share.min_price_increment),
                    lot=share.lot,
                    price=last_prices[figi]
                )
            )
    possible_positions.sort(key=lambda pos: pos.price * pos.lot)
    for pos in possible_positions:
        print(f'min_capital={pos.lot * pos.price}', pos)


if __name__ == '__main__':
    main()

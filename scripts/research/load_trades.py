import sys
import os

sys.path.append(os.getcwd())

import datetime
import pickle
import time
import tinkoff.invest as inv
from pathlib import Path
from itertools import count
from scripts.common.utils import read_config, to_moscow


config = read_config()

CACHE_DIR = Path("cache/")
CACHE_DIR.mkdir(exist_ok=True)


def save(obj, name: str):
    with open(CACHE_DIR / f"{name}.pickle", "wb") as f:
        pickle.dump(obj, f)


def load(name: str):
    with open(CACHE_DIR / f"{name}.pickle", "rb") as f:
        return pickle.load(f)


def exists(name: str) -> bool:
    return (CACHE_DIR / f"{name}.pickle").exists()


def download_operations():
    today = datetime.datetime.today()
    # START = datetime.datetime(year=2024, month=2, day=1)
    START = today - datetime.timedelta(days=30)
    LIMIT = 1000
    cursor = ""
    with inv.Client(token=config["runner"]["token"]) as client:
        positions = client.operations.get_positions(account_id=str(config['user']['account_id']))
        result = []
        for i in count(start=1):
            operations = client.operations.get_operations_by_cursor(
                inv.GetOperationsByCursorRequest(
                    account_id=str(config["user"]["account_id"]),
                    instrument_id=config["runner"]["figi"],
                    from_=START,
                    to=today,
                    limit=LIMIT,
                    operation_types=[inv.OperationType.OPERATION_TYPE_UNSPECIFIED],
                    state=inv.OperationState.OPERATION_STATE_UNSPECIFIED,
                    without_commissions=False,
                    without_trades=False,
                    without_overnights=False,
                    cursor=cursor,
                )
            )
            result.append(operations)
            if not operations.has_next:
                break
            cursor = operations.next_cursor
            print(f"Load operations: {i} ({len(operations.items)}) at {to_moscow(operations.items[-1].date)}")
            time.sleep(1)
    return positions, result


def main():
    CACHE = False
    if not CACHE or not exists("operations"):
        positions, operations = download_operations()
        save((positions, operations), "operations")
    else:
        positions, operations = load("operations")
    operations: list[inv.OperationItem] = sum(map(lambda x: x.items, operations), start=[])
    print(f'{len(operations)} operations')
    print(f'Positions: {positions}')


if __name__ == "__main__":
    main()

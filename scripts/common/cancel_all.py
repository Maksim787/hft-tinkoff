import sys
import os

sys.path.append(os.getcwd())

import tinkoff.invest as inv
from scripts.common.utils import parse_config


def cancel_all_orders(debug: bool = False):
    if debug:
        print("cancel_all_orders was called!")
        return
    config = parse_config()
    with inv.Client(token=config.token) as client:
        client.cancel_all_orders(account_id=config.account_id)
    print("Cancel orders: Success")


if __name__ == "__main__":
    cancel_all_orders()

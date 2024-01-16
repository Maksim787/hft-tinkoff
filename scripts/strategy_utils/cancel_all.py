import tinkoff.invest as inv
from common.utils import parse_config


def main():
    config = parse_config()
    with inv.Client(token=config.token) as client:
        client.cancel_all_orders(account_id=config.account_id)
    print('Success')


if __name__ == '__main__':
    main()

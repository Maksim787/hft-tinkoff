import sys
import os

sys.path.append(os.getcwd())

import tinkoff.invest as inv
from collections import Counter
from scripts.common.utils import parse_config


def main():
    config = parse_config()
    with inv.Client(token=config.token) as client:
        codes = Counter([i.class_code for i in client.instruments.shares().instruments])
        codes += Counter([i.class_code for i in client.instruments.etfs().instruments])
        codes += Counter([i.class_code for i in client.instruments.bonds().instruments])
        print("Class code counts:", codes)
        print("Most common codes:", [c[0] for c in codes.most_common(100)])


if __name__ == "__main__":
    main()

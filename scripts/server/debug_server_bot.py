import sys
import os

sys.path.append(os.getcwd())


import time
from itertools import count

# to check imports in the environment
import aiogram
from common.utils import read_config

config = read_config()


def main():
    print("Bot launched!")
    interrupt_counts = 0
    for i in count(start=1):
        try:
            print(f"Tick: {i}")
            time.sleep(1)
        except KeyboardInterrupt:
            print("Got KeyboardInterrupt")
            if interrupt_counts == 1:
                raise
            interrupt_counts += 1


if __name__ == "__main__":
    main()

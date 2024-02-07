import time
from itertools import count
import aiogram  # to check imports in the environment


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

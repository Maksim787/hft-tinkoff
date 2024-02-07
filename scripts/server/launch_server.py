import sys
import os

sys.path.append(os.getcwd())

import subprocess
import signal
import time
import datetime
import pytz
from pathlib import Path
from strategy_utils.cancel_all import cancel_all_orders
from common.utils import to_moscow
from common.utils import read_config

config = read_config()["server"]

# Find root directory
root = Path.cwd()
for i in range(10):
    if root.name == "hft-tinkoff":
        break
    root = root.parent
else:
    assert False, f"{Path.cwd()}"
print(f"Work in {root}\n")
os.chdir(root)

# Parse config
debug = config["debug"]
print(f"{debug = }")
bot_script_path = Path(root) / (config["bot_script_path"] if not debug else "scripts/server/temp_bot.py")
strategy_bin_path = Path(root) / config["strategy_bin_path"]
bot_logs_path = Path(root) / config["bot_logs_path"]
strategy_logs_path = Path(root) / config["strategy_logs_path"]
print(bot_script_path)
assert Path(bot_script_path).exists()
assert Path(bot_logs_path).parent.exists()
assert Path(strategy_logs_path).parent.exists()


def launch_telegram_bot(bot_logs_file):
    os.chdir(root / 'scripts')
    print("Launch telegram bot")
    telegram_bot_p = subprocess.Popen(["python", str(bot_script_path)], stdout=bot_logs_file, stderr=bot_logs_file)
    print("Launch telegram bot: success\n")
    os.chdir(root)
    return telegram_bot_p


def launch_strategy(strategy_logs_file):
    # Launch strategy
    print("Launch strategy")
    strategy_p = subprocess.Popen([str(strategy_bin_path)], stdout=strategy_logs_file, stderr=strategy_logs_file)
    print("Launch strategy: success\n")
    return strategy_p


def stop_process(process, name: str):
    print(f"Stop process {name}")
    if process.poll() is None:
        process.send_signal(signal.SIGINT)
    else:
        print(f"Stop process {name}: success (already stopped)\n")
        return
    time.sleep(1)
    if process.poll() is None:
        print(f"[WARNING] Kill process {name}")
        process.kill()
    time.sleep(1)
    assert process.poll() is not None
    print(f"Stop process {name}: success (stopped by signal)\n")


def main():
    n_reboots = 0
    while True:
        while True:
            now = datetime.datetime.utcnow().replace(tzinfo=pytz.utc).astimezone(pytz.timezone("Europe/Moscow"))
            if datetime.time(hour=10, minute=30) < now.time() < datetime.time(hour=18, minute=30) or datetime.time(hour=19, minute=30) < now.time() < datetime.time(hour=23, minute=40):
                break
            else:
                time.sleep(30)
        n_reboots += 1
        print(f'Reboot: {n_reboots = }')
        if n_reboots >= 5:
            return

        # Cancel orders
        print("\nCancel orders")
        cancel_all_orders(debug=debug)
        print("Cancel orders: success\n")

        # Open log files
        with open(bot_logs_path, "a") as bot_logs_file, open(strategy_logs_path, "a") as strategy_logs_file:
            # Launch telegram bot
            telegram_bot_p = launch_telegram_bot(bot_logs_file)
            strategy_p = launch_strategy(strategy_logs_file)

            try:
                # Wait for processes
                print("Wait for processes")
                while True:
                    if strategy_p.poll() is not None:
                        print(f"Strategy exitied with code: {strategy_p.returncode}")
                        break
                    if telegram_bot_p.poll() is not None:
                        print(f"Telegram bot exitied with code: {telegram_bot_p.returncode}")
                        telegram_bot_p = launch_telegram_bot(bot_logs_file)
                        time.sleep(1)
                    time.sleep(1)
            except KeyboardInterrupt:
                print("\n\nSend stop signal to processes\n")
                stop_process(telegram_bot_p, f"telegram_bot {bot_script_path}")
                stop_process(strategy_p, f"strategy {strategy_bin_path}")
                return
            finally:
                assert telegram_bot_p.poll() is not None
                assert strategy_p.poll() is not None
                print("Processes are stopped: success\n")
                bot_logs_file.close()
                strategy_logs_file.close()
            # ps ax | grep -E 'python|grid_trading'


if __name__ == "__main__":
    main()
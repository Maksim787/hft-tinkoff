import sys
import os

sys.path.append(os.getcwd())

import socket
from common.utils import read_config


config = read_config()


def main():
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect(("localhost", config["telegram"]["port"]))
    while True:
        try:
            message = input("Message: ")
        except KeyboardInterrupt:
            break
        print("Send...")
        client_socket.send(message.encode())
        print("Sent")
    client_socket.close()


if __name__ == "__main__":
    main()

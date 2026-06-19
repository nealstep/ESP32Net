#!/usr/bin/env python3

from argparse import ArgumentParser
from socket import socket

from socket import AF_INET, SOCK_DGRAM

from secret import log_port, log_host

parser = ArgumentParser(description="UDP Log Sender")
parser.add_argument(
    "-t",
    "--target-ip",
    type=str,
    default=log_host,
    help="Target IP address to send log message to",
)
parser.add_argument(
    "-p",
    "--target-port",
    type=int,
    default=log_port,
    help="Target port to send log messageto",
)
parser.add_argument(
    "-m",
    "--message",
    type=str,
    default="Hello, UDP Receiver!",
    help="Message to send",
)
args = parser.parse_args()

sock = socket(AF_INET, SOCK_DGRAM)

target = (args.target_ip, args.target_port)
mesg = args.message.encode()
sock.sendto(mesg, target)
print("Sent message\n")
sock.close()

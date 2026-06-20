#!/usr/bin/env python3

from argparse import ArgumentParser
from configparser import ConfigParser
from Crypto.Cipher import AES
from socket import socket

from configparser import Error as CPError

from binascii import unhexlify
from os.path import dirname, abspath, isfile

from os.path import join as pjoin

from Crypto.Random import get_random_bytes

from socket import AF_INET, SOCK_DGRAM, SO_BROADCAST, SOL_SOCKET

# defaults
broadcast = "255.255.255.255"
message = "Hello, UDP Receiver!"

# get ini file
config = ConfigParser()
py_file_dir = dirname(abspath(__file__))
secret_ini = pjoin(py_file_dir, "../secret.ini")
if not isfile(secret_ini):
    print(f"Ini file not found: {secret_ini}")
    exit(1)
try:
    config.read(secret_ini, encoding="utf-8")
except CPError as e:
    print(f"Error reading INI file: {e}")
    exit(1)
udp_data_port = config.getint("secret", "udp_data_port")
hex_key = config.get("secret", "aes_key")
cleaned = hex_key.removeprefix('"').removesuffix('"')
aes_key = unhexlify(cleaned)

parser = ArgumentParser(description="UDP Log Listener")
parser.add_argument(
    "-p",
    "--port",
    default=udp_data_port,
    type=int,
    help="UDP port to send to",
)
parser.add_argument(
    "-i", "--ip", default=broadcast, help="IP address to send to"
)
parser.add_argument(
    "-e", "--encrypted", action="store_true", help="Use encryption"
)
parser.add_argument(
    "-m",
    "--message",
    type=str,
    default=message,
    help="Message to send",
)
args = parser.parse_args()
ip = args.ip
port = args.port
enc = args.encrypted

sock = socket(AF_INET, SOCK_DGRAM)
sock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
target = (args.ip, args.port)
mesg = args.message.encode()
if enc:
    nonce = get_random_bytes(12)
    cipher = AES.new(aes_key, AES.MODE_GCM, nonce=nonce)
    ciphertext, tag = cipher.encrypt_and_digest(mesg)
    packet = bytearray(len(cipher.nonce) + len(tag) + len(ciphertext))
    packet[0:12] = cipher.nonce
    packet[12:28] = tag
    packet[28:] = ciphertext
    mesg = packet
sock.sendto(mesg, target)
print("Sent message\n")
sock.close()

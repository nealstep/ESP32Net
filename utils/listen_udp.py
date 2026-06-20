#!/usr/bin/env python3

from argparse import ArgumentParser
from configparser import ConfigParser
from Crypto.Cipher import AES
from datetime import datetime
from socket import socket

from configparser import Error as CPError

from binascii import unhexlify
from os.path import abspath, dirname, isfile

from os.path import join as pjoin


from socket import AF_INET, SOCK_DGRAM, SOL_SOCKET, SO_REUSEADDR, SO_BROADCAST

# listener on all interfaces
listen_address = "0.0.0.0"

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
    help="UDP port to listen on",
)
parser.add_argument(
    "-i", "--ip", default=listen_address, help="IP address to bind to"
)
parser.add_argument(
    "-e", "--encrypted", action="store_true", help="Use encryption"
)
args = parser.parse_args()
ip = args.ip
port = args.port
enc = args.encrypted

sock = socket(AF_INET, SOCK_DGRAM)
sock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
sock.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)
sock.bind((ip, port))
print(f"UDP listener started on {ip}:{port}")

while True:
    data, addr = sock.recvfrom(1024)
    asof = datetime.now()
    asof_str = asof.strftime("%Y-%m-%d@%H:%M:%S")
    if (enc):
        if len(data) < 28:
            print(f"Malformed packet received from {addr} (Too short).")
            continue
        try:
            iv = data[:12]
            tag = data[12:28]
            ciphertext = data[28:]
            cipher = AES.new(aes_key, AES.MODE_GCM, nonce=iv)
            decrypted_bytes = cipher.decrypt_and_verify(ciphertext, tag)
            plaintext_str = decrypted_bytes.decode("utf-8")
            print(f"{asof_str}^{addr[0]}^{plaintext_str}")
        except ValueError as e:
            print(
                f"Decryption failed or corrupt data from {addr} {e}"
            )
        except Exception as e:
            print(f"Unexpected processing error: {e}")
    else:
        print(f"{asof_str}^{addr[0]}^{data.decode()}")

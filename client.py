#!/usr/bin/env python

import socket
import struct

# -------------------------------------------------- GLOBAL VARIABLES --------------------------------------------------

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

config_data = {}

# Dictionaries
packet_dictionary = {
    0x00: 'SUBS_REQ',  # Subscription Request
    0X01: 'SUBS_ACK',  # Subscription Acknowledgement
    0x02: 'SUBS_REJ',  # Subscription Rejection
    0x03: 'SUBS_INFO',  # Subscription Information
    0x04: 'INFO_ACK',  # Information Acknowledgement
    0x05: 'SUBS_NACK',  # Subscription Negative Acknowledgement
}

client_states_dictionary = {
    0xa0: 'DISCONNECTED',  # Controller disconnected
    0xa1: 'NOT_SUBSCRIBED',  # Controller connected and not subscribed
    0xa2: 'WAIT_ACK_SUBS',  # Waiting for acknowledgement of the 1st subscription packet
    0xa3: 'WAIT_INFO',  # Server waiting for SUBS_INFO packet
    0xa4: 'WAIT_ACK_INFO',  # Waiting for acknowledgement of the 2nd subscription packet
    0xa5: 'SUBSCRIBED',  # Controller subscribed, without sending HELLO
    0xa6: 'SEND_HELLO',  # Controller sending HELLO packets
}

# Client initial state
client_state = 'NOT_SUBSCRIBED'  # 0xa1 = NOT_SUBSCRIBED


# Define UDP Packet structure
class UDPPacket:
    def __init__(self):
        self.type = None  # type (1 byte) == unsigned char
        self.MAC = None  # MAC (13 bytes) == char
        self.random = None  # random (9 bytes) == char
        self.data = None  # data (80 bytes) == char


packet = UDPPacket()

# Timers
t = 1
u = 2
n = 7
o = 3
p = 3
q = 3

# Send packet to server info
host = socket.gethostbyname(config_data.get('Server'))
port = int(config_data.get('Srv-UDP'))
buffer_size = 1 + 13 + 9 + 80


# ----------------------------------------------------------------------------------------------------------------------

# Read file 'client.cfg' and save the information (Name, Situation, Elements, MAC, Local, Server, Srv-UDP) inside
# config_data
def read_config():
    file = open('client.cfg', 'r')
    try:
        for line in file:
            key, value = line.strip().split(' = ')

            config_data[key] = value
    finally:
        file.close()


def send_subscription():
    global client_state  # 0xa1 = NOT_SUBSCRIBED
    # Init UDPPacket:
    packet.type = 0x00  # 0x00 = SUBS_REQ
    packet.MAC = config_data.get('MAC')
    packet.random = "00000000"
    packet.data = f"{config_data.get('Name')},{config_data.get('Situation')}"

    # UDPPacket to byte-like object -> 'B13s9s80s' defines packet format (B=1byte,13s=13char,9s=9char,80s=80char)
    pack = struct.pack('B13s9s80s', packet.type, packet.MAC.encode(), packet.random.encode(), packet.data.encode())

    sock.sendto(pack, (host, port))

    # Change client state after sending packet
    client_state = 'WAIT_ACK_SUBS'  # 0xa2 = WAIT_ACK_SUBS


def subscription():
    n_packet = 0
    n_subs_proc = 0
    while client_state == 'WAIT_ACK_SUBS' or n_subs_proc <= o:
        time = 0
        while client_state == 'WAIT_ACK_SUBS' or n_packet <= n:
            if n_packet < p:
                time = t
            elif n_packet >= p and time < q*t:
                time += t
            if n_packet == n:
                time = u

            sock.settimeout(time)
            try:
                send_subscription()
                sock.recvfrom(buffer_size)

            except socket.timeout:
                n_packet += 1

        n_subs_proc += 1


def print_process():
    print("\n-------------- IDENTIFY CLIENT TO THE SERVER: --------------\n")

    print(f"SUBSCRIPTION REQUEST:\n"
          f"- Packet Type: {packet.type} == '{packet_dictionary.get(packet.type)}'\n"
          f"- MAC Adress: {packet.MAC}\n"
          f"- Random Number: {packet.random}\n"
          f"- Data: {packet.data}\n")
    print(f"SENT PACKET:\n"
          f"- Host: {config_data.get('Server')} == {socket.gethostbyname(config_data.get('Server'))}\n"
          f"- Port: {config_data.get('Srv-UDP')}")

    print(f"\nClient state: {client_state}\n")


if __name__ == '__main__':
    read_config()
    subscription()
    print_process()

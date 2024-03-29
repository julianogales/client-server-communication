#!/usr/bin/env python

import socket
import struct
import time

# -------------------------------------------------- GLOBAL VARIABLES --------------------------------------------------

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

config_data = {}

# Dictionaries
packet_dictionary = {
    0x00: 'SUBS_REQ',  # Subscription Request
    0x01: 'SUBS_ACK',  # Subscription Acknowledgement
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
rcv_packet = UDPPacket()

# Timers
t = 1
u = 2
n = 7
o = 3
p = 3
q = 3

# Send packet to server info
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

    host = socket.gethostbyname(config_data.get('Server'))
    port = int(config_data.get('Srv-UDP'))
    sock.sendto(pack, (host, port))

    # Change client state after sending packet
    client_state = 'WAIT_ACK_SUBS'  # 0xa2 = WAIT_ACK_SUBS


def read_packet():
    (rcv_pack, (rcv_host, rcv_port)) = sock.recvfrom(buffer_size)
    rcv_packet.type, rcv_packet.MAC, rcv_packet.random, rcv_packet.data = struct.unpack('B13s9s80s', rcv_pack)

    # Obtain the real data by converting or removing extra bytes
    rcv_packet.MAC = rcv_packet.MAC.decode().strip(b'\x00'.decode())
    rcv_packet.random = rcv_packet.random.decode().strip(b'\x00'.decode())
    rcv_packet.data = rcv_packet.data.split(b'\x00')[0].decode()

    return rcv_packet.type == 0x01 and client_state == 'WAIT_ACK_SUBS'  # 0x01 = SUBS_ACK ; 0xa2 = WAIT_ACK_SUBS


def send_info():
    global client_state
    tcp_port = config_data.get('Local-TCP')
    devices = config_data.get('Elements')
    data = f"{tcp_port},{devices}"

    pack = struct.pack('B13s9s80s', 0x03, packet.MAC.encode(), rcv_packet.random.encode(), data.encode())

    host = socket.gethostbyname(config_data.get('Server'))
    port = int(rcv_packet.data)
    sock.sendto(pack, (host, port))

    # Change client state after sending packet
    client_state = 'WAIT_ACK_INFO'  # 0xa3 = WAIT_ACK_INFO


def subscription():
    n_subs_proc = 1
    received_packet = False
    while not received_packet and n_subs_proc <= o:
        print(f"Subscription process {n_subs_proc}:")
        timeout = 0
        n_packet = 0
        while not received_packet and n_packet < n:
            if n_packet < p:
                timeout = t
            elif n_packet >= p and timeout < q * t:
                timeout += t

            send_subscription()
            n_packet += 1
            print(f"Packet {n_packet} sent.")

            sock.settimeout(timeout)
            try:
                if read_packet():
                    print("Server response received!")
                    received_packet = True

            except socket.timeout:
                print(f"Timeout({timeout}s)")

        n_subs_proc += 1
        if n_packet == n and n_subs_proc <= o:
            print(f"Unable to complete subscription process {n_subs_proc - 1}.")
            print(f"\nWaiting {u} seconds to restart the subscription process...\n")
            time.sleep(u)

    if received_packet:
        send_info()
        print("[SUBS_INFO] packet sent to server.")

    else:
        print(f"Unable to complete subscription process {n_subs_proc - 1}.")
        print("\nSubscription process was cancelled. Server could not be reached.")


def print_process():
    print("\n-------------- IDENTIFY CLIENT TO THE SERVER: --------------\n")

    print(f"SUBSCRIPTION REQUEST:\n"
          f"- Packet Type: {packet.type} == '{packet_dictionary.get(packet.type)}'\n"
          f"- MAC Address: {packet.MAC}\n"
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

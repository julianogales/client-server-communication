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
    0x10: 'HELLO',  # Sending HELLO
    0x11: 'HELLO_REJ',  # Rejection Packet HELLO
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
host_1 = None
mac_1 = None
host_2 = None
mac_2 = None

# Timers
t = 1
u = 2
n = 7
o = 3
p = 3
q = 3
v = 2
r = 2

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


def read_packet(data_action):
    global host_1, mac_1, host_2, mac_2

    (rcv_pack, (rcv_host, rcv_port)) = sock.recvfrom(buffer_size)
    rcv_packet.type, rcv_packet.MAC, rcv_packet.random, rcv_packet.data = struct.unpack('B13s9s80s', rcv_pack)

    # Obtain the real data by converting or removing extra bytes
    rcv_packet.MAC = rcv_packet.MAC.decode().strip(b'\x00'.decode())
    rcv_packet.random = rcv_packet.random.decode().strip(b'\x00'.decode())
    rcv_packet.data = rcv_packet.data.split(b'\x00')[0].decode()

    if data_action == 'store':
        host_1 = rcv_host
        mac_1 = rcv_packet.MAC
    else:
        host_2 = rcv_host
        mac_2 = rcv_packet.MAC

    # Classify packets
    if rcv_packet.type == 0x01 and client_state == 'WAIT_ACK_SUBS':  # 0x01 = SUBS_ACK ; 0xa2 = WAIT_ACK_SUBS
        return 'SUBS_ACK'
    elif rcv_packet.type == 0x05:  # 0x05 = SUBS_NACK
        return 'SUBS_NACK'
    elif rcv_packet.type == 0x02:  # 0x02 = SUBS_REJ
        return 'SUBS_REJ'
    elif rcv_packet.type == 0x04 and client_state == 'WAIT_ACK_INFO':  # 0x04 = INFO_ACK
        return 'INFO_ACK'
    else:
        return 'Incorrect'


def classify_packet(packet_type):
    global client_state
    print(f"[{packet_type}] packet received from server.")

    if packet_type == 'SUBS_ACK':
        subscription('SUBS_INFO')

    elif packet_type == 'SUBS_NACK':
        client_state = 'NOT_SUBSCRIBED'  # 0xa1 = NOT_SUBSCRIBED
        print(f"Update client state to [{client_state}]")

    elif packet_type == 'SUBS_REJ':
        client_state = 'NOT_SUBSCRIBED'
        print(f"Update client state to [{client_state}]")
        subscription('SUBS_REQ')

    elif packet_type == 'INFO_ACK':
        if verify_server_id():
            client_state = 'SUBSCRIBED'
            print(f"Update client state to [{client_state}]")
            print("Subscription phase successfully completed!")
        else:
            client_state = 'NOT_SUBSCRIBED'
            print(f"Update client state to [{client_state}]")
            subscription('SUBS_REQ')

    else:  # packet_type == 'Incorrect'
        client_state = 'NOT_SUBSCRIBED'
        print(f"Update client state to [{client_state}]")
        subscription('SUBS_REQ')


def send_info():
    global client_state
    tcp_port = config_data.get('Local-TCP')
    devices = config_data.get('Elements')
    data = f"{tcp_port},{devices}"

    pack = struct.pack('B13s9s80s', 0x03, packet.MAC.encode(), rcv_packet.random.encode(), data.encode())

    host = socket.gethostbyname(config_data.get('Server'))
    port = int(rcv_packet.data)
    sock.sendto(pack, (host, port))


def verify_server_id():
    return host_1 == host_2 and mac_1 == mac_2


def subscription(sent_packet):
    global client_state
    n_subs_proc = 1
    packet_type = None
    while packet_type is None and n_subs_proc <= o:
        if sent_packet == 'SUBS_REQ':
            print(f"Subscription process {n_subs_proc}:")
        timeout = 0
        n_packet = 0
        while packet_type is None and n_packet < n:
            if n_packet < p:
                timeout = t
            elif n_packet >= p and timeout < q * t:
                timeout += t

            if sent_packet == 'SUBS_REQ':
                send_subscription()
                n_packet += 1
                print(f"[SUBS_REQ] packet {n_packet} sent to server.")
            elif sent_packet == 'SUBS_INFO':
                send_info()
                n_packet += 1
                print(f"[SUBS_INFO] packet {n_packet} sent to server.")
                client_state = 'WAIT_ACK_INFO'  # 0xa3 = WAIT_ACK_INFO
                print(f"Update client state to [{client_state}]")

            sock.settimeout(timeout)
            try:
                if sent_packet == 'SUBS_REQ':
                    packet_type = read_packet('store')
                elif sent_packet == 'SUBS_INFO':
                    packet_type = read_packet('compare')
                if packet_type is not None:
                    print("Server response received!")

            except socket.timeout:
                print(f"Timeout({timeout}s)")

        n_subs_proc += 1
        if n_packet == n and n_subs_proc <= o:
            print(f"Unable to complete subscription process {n_subs_proc - 1}.")
            print(f"\nWaiting {u} seconds to restart the subscription process...\n")
            time.sleep(u)

    # Packet received
    if packet_type is not None:
        classify_packet(packet_type)

    # Packet not received
    else:
        print(f"Unable to complete subscription process {n_subs_proc - 1}.")
        print("\nSubscription process was cancelled. Server could not be reached.")


if __name__ == '__main__':
    read_config()
    subscription('SUBS_REQ')

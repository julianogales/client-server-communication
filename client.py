#!/usr/bin/env python

import os
import signal
import socket
import struct
import threading
import time

# -------------------------------------------------- GLOBAL VARIABLES --------------------------------------------------

sock_udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock_tcp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

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

# Packet data
host_server = None
mac_server = None
random_server = None
host_info = None
mac_info = None
host_hello = None
mac_hello = None
random_hello = None
data_hello = None

# Timers
t = 1
u = 2
n = 7
o = 3
p = 3
q = 3
v = 2
r = 2
s = 3

# Send packet to server info
buffer_size = 1 + 13 + 9 + 80

# Threads
terminate_thread = False


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
    sock_udp.sendto(pack, (host, port))

    # Change client state after sending packet
    client_state = 'WAIT_ACK_SUBS'  # 0xa2 = WAIT_ACK_SUBS


def read_packet(data_action):
    global host_server, mac_server, random_server, host_info, mac_info, host_hello, mac_hello, random_hello, data_hello

    (rcv_pack, (rcv_host, rcv_port)) = sock_udp.recvfrom(buffer_size)
    rcv_packet.type, rcv_packet.MAC, rcv_packet.random, rcv_packet.data = struct.unpack('B13s9s80s', rcv_pack)

    # Obtain the real data by converting or removing extra bytes
    rcv_packet.MAC = rcv_packet.MAC.decode().strip(b'\x00'.decode())
    rcv_packet.random = rcv_packet.random.decode().strip(b'\x00'.decode())
    rcv_packet.data = rcv_packet.data.split(b'\x00')[0].decode()

    if data_action == 'store':
        host_server = rcv_host
        mac_server = rcv_packet.MAC
        random_server = rcv_packet.random
    elif data_action == 'info':
        host_info = rcv_host
        mac_info = rcv_packet.MAC
    elif data_action == 'hello':
        host_hello = rcv_host
        mac_hello = rcv_packet.MAC
        random_hello = rcv_packet.random
        data_hello = rcv_packet.data

    return packet_dictionary.get(rcv_packet.type)


def classify_packet(packet_type):
    global client_state

    if packet_type == 'SUBS_ACK' and client_state == 'WAIT_ACK_SUBS':
        print(f"[{packet_type}] packet received from server.")
        subscription('SUBS_INFO')

    elif packet_type == 'SUBS_NACK':
        print(f"[{packet_type}] packet received from server.")
        client_state = 'NOT_SUBSCRIBED'
        print(f"Update client state to [{client_state}]")

    elif packet_type == 'SUBS_REJ':
        print(f"[{packet_type}] packet received from server.")
        client_state = 'NOT_SUBSCRIBED'
        print(f"Update client state to [{client_state}]")
        subscription('SUBS_REQ')

    elif packet_type == 'INFO_ACK' and client_state == "WAIT_ACK_INFO" and verify_server_id():
        print(f"[{packet_type}] packet received from server.")
        client_state = 'SUBSCRIBED'
        print(f"Update client state to [{client_state}]")
        print("Subscription phase successfully completed!")
        communication()

    elif packet_type == 'HELLO':
        # print(f"[{packet_type}] packet received from server.")
        if client_state == 'SUBSCRIBED' and verify_server_hello():  # First HELLO packet
            client_state = 'SEND_HELLO'
            print(f"Update client state to [{client_state}]")

        if not verify_server_hello() or (not client_state == 'SUBSCRIBED' and not client_state == 'SEND_HELLO'):
            send_hello(0x11)  # 0x11 = HELLO_REJ
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
    sock_udp.sendto(pack, (host, port))


def verify_server_id():
    return host_server == host_info and mac_server == mac_info


def verify_server_hello():
    return (host_hello == host_server and mac_hello == mac_server and random_hello == random_server
            and data_hello == packet.data)


def subscription(sent_packet):
    global client_state
    n_subs_proc = 1
    packet_type = None
    while packet_type is None and n_subs_proc <= o:
        if sent_packet == 'SUBS_REQ':
            print(f"\nSubscription process {n_subs_proc}:")
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

            sock_udp.settimeout(timeout)
            try:
                if sent_packet == 'SUBS_REQ':
                    packet_type = read_packet('store')
                elif sent_packet == 'SUBS_INFO':
                    packet_type = read_packet('info')
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


def send_hello(packet_type_num):
    hello_pack = struct.pack('B13s9s80s', packet_type_num, packet.MAC.encode(), random_server.encode(),
                             packet.data.encode())

    port = int(config_data.get('Srv-UDP'))

    if packet_dictionary.get(packet_type_num) == 'HELLO_REJ':
        sock_udp.sendto(hello_pack, (host_server, port))

    n_packet = 0
    while not terminate_thread:
        sock_udp.sendto(hello_pack, (host_server, port))
        n_packet += 1
        # print(f"[HELLO] packet {n_packet} sent to server.")
        # print(f"Waiting {v}s...")
        time.sleep(v)


def system_input():
    while client_state == 'SEND_HELLO' and not terminate_thread:
        command = input()
        parts = command.split(' ')

        if client_state == 'SEND_HELLO':
            if command == 'stat':
                print("Showing id date from the controller (MAC, name and situation), all the controller devices and "
                      "their actual value...")
            elif parts[0] == 'set' and len(parts) >= 3:
                device_name = parts[1]
                value = parts[2]
                print("Simulating the read of a sensor value or the controller state and changing the value associated "
                      "to the device <device_name> to <value>...")
            elif parts[0] == 'send' and len(parts) >= 1:
                device_name = parts[1]
                print("Sending the value associated to the device <device_name> to the server...")
            elif command == 'quit':
                print("Terminating client execution. Closing communication ports and terminating all the processes.")
                os.kill(os.getppid(), signal.SIGINT)
            else:
                print("Unknown command. Please enter one of the following commands:"
                      "\n- stat\n- set <device_name> <value>\n- send <device_name>\n- quit")


def communication():
    print("\nCommunication process:")
    global terminate_thread
    send_thread = threading.Thread(target=send_hello, args=(0x10,))
    send_thread.start()

    # First HELLO packet
    timeout = v * r
    if client_state == 'SUBSCRIBED':
        sock_udp.settimeout(timeout)
        try:
            packet_type = read_packet('hello')
            classify_packet(packet_type)
        except socket.timeout:
            print(f"Communication failure. No [HELLO] packet has arrived within {timeout}s")
            terminate_thread = True
            send_thread.join()
            classify_packet('NotHello')

    # Following HELLOs packets
    if client_state == 'SEND_HELLO':
        # Open TCP port:
        sock_tcp.bind((host_server, int(config_data.get('Local-TCP'))))
        print("Connection TCP client-server has been established.")

        system_thread = threading.Thread(target=system_input)
        system_thread.start()

        # Receive following HELLOs and count strikes:
        strike_hello_miss = 0
        timeout = v
        strike = True
        while client_state == 'SEND_HELLO' and strike_hello_miss < s:
            if not strike:
                strike_hello_miss = 0

            sock_udp.settimeout(timeout)
            try:
                packet_type = read_packet('hello')
                classify_packet(packet_type)
                strike = False
            except socket.timeout:
                strike_hello_miss += 1
                strike = True

        sock_tcp.close()
        if strike_hello_miss == 3:
            print(f"STRIKE 3. No [HELLO] packets received within {3 * timeout}s")
            terminate_thread = True
            send_thread.join()
            classify_packet('NotHello')


if __name__ == '__main__':
    read_config()
    subscription('SUBS_REQ')

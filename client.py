#!/usr/bin/env python

import os
import signal
import socket
import struct
import sys
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

# Entry parameters
debug = False
file_name = 'client.cfg'


# ----------------------------------------------------------------------------------------------------------------------
def read_entry_parameters():
    global debug, file_name

    if len(sys.argv) > 1:
        arg = sys.argv[1]
        if arg == '-d':
            debug = True
            print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
                  "Llegits paràmetres línia de comandes")
        elif arg == '-c':
            if len(sys.argv) > 2:
                file_name = sys.argv[2]
            else:
                print(f"{time.strftime('%H:%M:%S', time.localtime())}: ERROR.  => "
                      f"No es pot obrir l'arxiu de configuració:  ")
                sys.exit()


# Read file 'client.cfg' and save the information (Name, Situation, Elements, MAC, Local, Server, Srv-UDP) inside
# config_data
def read_config():
    try:
        file = open(file_name, 'r')
        try:
            for line in file:
                key, value = line.strip().split(' = ')

                config_data[key] = value
        finally:
            file.close()
            if debug:
                print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
                      "Llegits paràmetres arxius de configuració")
    except Exception:
        print(f"{time.strftime('%H:%M:%S', time.localtime())}: ERROR => "
              f"No es pot obrir l'arxiu de configuració: {sys.argv[2]}")
        sys.exit()


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

    if debug:
        print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
              f"Enviat: bytes={buffer_size}, comanda={packet_dictionary.get(packet.type)}, mac={packet.MAC}, "
              f"rndm={packet.random}, dades={packet.data}")

    # Change client state after sending packet
    if client_state != 'WAIT_ACK_SUBS':
        client_state = 'WAIT_ACK_SUBS'  # 0xa2 = WAIT_ACK_SUBS
        print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
              f"Controlador passa a l'estat: {client_state}")


def read_packet(data_action):
    global host_server, mac_server, random_server, host_info, mac_info, host_hello, mac_hello, random_hello, data_hello

    (rcv_pack, (rcv_host, rcv_port)) = sock_udp.recvfrom(buffer_size)
    rcv_packet.type, rcv_packet.MAC, rcv_packet.random, rcv_packet.data = struct.unpack('B13s9s80s', rcv_pack)

    # Obtain the real data by converting or removing extra bytes
    rcv_packet.MAC = rcv_packet.MAC.decode().strip(b'\x00'.decode())
    rcv_packet.random = rcv_packet.random.decode().strip(b'\x00'.decode())
    rcv_packet.data = rcv_packet.data.split(b'\x00')[0].decode()

    print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
          f"Rebut: bytes={buffer_size}, comanda={packet_dictionary.get(rcv_packet.type)}, mac={rcv_packet.MAC}, "
          f"rndm={rcv_packet.random}, dades={rcv_packet.data}")

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
        subscription('SUBS_INFO')

    elif packet_type == 'SUBS_NACK':
        client_state = 'NOT_SUBSCRIBED'
        print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
              f"Controlador passa a l'estat: {client_state}")

    elif packet_type == 'SUBS_REJ':
        client_state = 'NOT_SUBSCRIBED'
        subscription('SUBS_REQ')

    elif packet_type == 'INFO_ACK' and client_state == "WAIT_ACK_INFO" and verify_server_id():
        if debug:
            print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
                  "Acceptada la subscripció del controlador del servidor")
        client_state = 'SUBSCRIBED'
        print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
              f"Controlador passa a l'estat: {client_state}")
        communication()

    elif packet_type == 'HELLO':
        if client_state == 'SUBSCRIBED' and verify_server_hello():  # First HELLO packet
            client_state = 'SEND_HELLO'
            print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
                  f"Controlador passa a l'estat: {client_state}")

        if not verify_server_hello() or (not client_state == 'SUBSCRIBED' and not client_state == 'SEND_HELLO'):
            send_hello(0x11)  # 0x11 = HELLO_REJ
            client_state = 'NOT_SUBSCRIBED'
            subscription('SUBS_REQ')

    else:  # packet_type == 'Incorrect'
        client_state = 'NOT_SUBSCRIBED'
        subscription('SUBS_REQ')


def send_info():
    tcp_port = config_data.get('Local-TCP')
    devices = config_data.get('Elements')
    data = f"{tcp_port},{devices}"

    pack = struct.pack('B13s9s80s', 0x03, packet.MAC.encode(), rcv_packet.random.encode(), data.encode())

    host = socket.gethostbyname(config_data.get('Server'))
    port = int(rcv_packet.data)
    sock_udp.sendto(pack, (host, port))

    if debug:
        print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
              f"Enviat: bytes={buffer_size}, comanda={packet_dictionary.get(0x03)}, mac={packet.MAC}, "
              f"rndm={rcv_packet.random}, dades={data}")


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
        client_state = 'NOT_SUBSCRIBED'
        if sent_packet == 'SUBS_REQ':
            print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
                  f"Controlador en l'estat: {client_state}, procés de subscripció: {n_subs_proc}")
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
            elif sent_packet == 'SUBS_INFO':
                send_info()
                n_packet += 1
                client_state = 'WAIT_ACK_INFO'  # 0xa3 = WAIT_ACK_INFO
                print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
                      f"Controlador passa a l'estat: {client_state}")

            sock_udp.settimeout(timeout)
            try:
                if sent_packet == 'SUBS_REQ':
                    packet_type = read_packet('store')
                elif sent_packet == 'SUBS_INFO':
                    packet_type = read_packet('info')

            except socket.timeout:
                pass

        n_subs_proc += 1
        if n_packet == n and n_subs_proc <= o:
            time.sleep(u)

    # Packet received
    if packet_type is not None:
        classify_packet(packet_type)

    # Packet not received
    else:
        print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
              f"Superat el nombre de processos de subscripció ( {n_subs_proc - 1} )")


def send_hello(packet_type_num):
    hello_pack = struct.pack('B13s9s80s', packet_type_num, packet.MAC.encode(), random_server.encode(),
                             packet.data.encode())

    port = int(config_data.get('Srv-UDP'))

    if packet_dictionary.get(packet_type_num) == 'HELLO_REJ':
        sock_udp.sendto(hello_pack, (host_server, port))

    n_packet = 0
    while not terminate_thread:
        sock_udp.sendto(hello_pack, (host_server, port))
        if debug:
            print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
                  f"Enviat: bytes={buffer_size}, comanda={packet_dictionary.get(packet_type_num)}, mac={packet.MAC}, "
                  f"rndm={random_server}, dades={packet.data}")
        n_packet += 1
        time.sleep(v)


def system_input():
    info_devices = []
    devices = config_data.get('Elements').split(';')
    dev_value = "NONE"

    for device in devices:
        info_devices.append((device, dev_value))

    while client_state == 'SEND_HELLO' and not terminate_thread:
        command = input()
        parts = command.split(' ')

        if client_state == 'SEND_HELLO':
            if command == 'stat':
                print("******************** DADES CONTROLADOR ********************")
                print(f"  MAC: {config_data.get('MAC')}, Nom: {config_data.get('Name')}, Situació: "
                      f"{config_data.get('Situation')}")

                print(f"\n   Estat: {client_state}\n")

                print("    Dispos.      valor\n    -------      ------")

                for device in info_devices:
                    dev, val = device
                    print(f"    {dev}      {val}")

                print("\n***********************************************************")

            elif parts[0] == 'set':
                if len(parts) >= 3:
                    device_name = parts[1]
                    value = parts[2]
                    exists = False
                    for device in info_devices:
                        dev, val = device
                        if dev == device_name:
                            exists = True
                            device.val = value
                    if not exists:
                        print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
                              f"Element: [{device_name}] no pertany al controlador")
                else:
                    print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
                          f"Error de sintàxi. (set <element> <valor>)")

            elif parts[0] == 'send':
                if len(parts) >= 1:
                    device_name = parts[1]
                    print("Sending the value associated to the device <device_name> to the server...")
                else:
                    print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
                          f"Error de sintàxi. (send <element>)")

            elif command == 'quit':
                print("Terminating client execution. Closing communication ports and terminating all the processes.")
                os.kill(os.getppid(), signal.SIGINT)

            else:
                print("Unknown command. Please enter one of the following commands:"
                      "\n- stat\n- set <device_name> <value>\n- send <device_name>\n- quit")


def communication():
    global client_state

    if debug:
        print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
              f"Creat procés per l'enviament periòdic de HELLO")
    global terminate_thread
    send_thread = threading.Thread(target=send_hello, args=(0x10,))
    send_thread.start()

    # First HELLO packet
    timeout = v * r
    if client_state == 'SUBSCRIBED':
        sock_udp.settimeout(timeout)
        if debug:
            print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
                  f"Establert temporitzador per enviament de HELLO")
        try:
            packet_type = read_packet('hello')
            classify_packet(packet_type)
        except socket.timeout:
            terminate_thread = True
            send_thread.join()
            classify_packet('NotHello')

    # Following HELLOs packets
    if client_state == 'SEND_HELLO':
        # Open TCP port:
        sock_tcp.bind((host_server, int(config_data.get('Local-TCP'))))
        if debug:
            print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
                  f"Obert port TCP {config_data.get('Local-TCP')} per la comunicació amb el servidor")

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
        if debug:
            print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
                  f"Tancat socket TCP per la comunicació amb el servidor")

        if strike_hello_miss == 3:
            terminate_thread = True
            send_thread.join()
            client_state = 'NOT_SUBSCRIBED'
            print(f"{time.strftime('%H:%M:%S', time.localtime())}: MSG.  => "
                  f"Controlador passa a l'estat: {client_state} (Sense resposta a 3 HELLOs)")
            classify_packet('NotHello')


if __name__ == '__main__':
    read_entry_parameters()
    read_config()
    if debug:
        print(f"{time.strftime('%H:%M:%S', time.localtime())}: DEBUG => "
              f"Inici bucle de servei equip: {config_data.get('Name')}")
    subscription('SUBS_REQ')

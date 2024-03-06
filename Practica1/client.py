#!/usr/bin/env python

import socket

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
client_state = client_states_dictionary.get(0xa1)  # client_state = NOT_SUBSCRIBED

# Define UDP Packet structure
class UDPPacket:
    def __init__(self):
        self.type = None  # type (1 byte) == unsigned char
        self.MAC = None  # MAC (13 bytes) == char
        self.random = None  # random (9 bytes) == char
        self.data = None  # data (80 bytes) == char

packet = UDPPacket()

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


def subscription():
    packet.type = 


if __name__ == '__main__':
    read_config()
    print(client_state)

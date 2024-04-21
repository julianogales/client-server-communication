/* Libraries */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

/* Constants */
#define X 3
#define V 2
#define R 2


typedef struct {
    unsigned char type;
    char mac[13];
    char random_number[9];
    char data[80];
} Packet_Udp;

typedef struct {
    char name[20];
    char ip[20];
    char mac[20];
    char state[20];
    char random_number[20];
    char situation[20];
    char devices[200];
    bool hello_received;
    int consecutive_lost_packets;
    int tcp_port;
} Controller;

typedef struct {
    char name[20];
    char mac[20];
    int udp_port;
    int tcp_port;
} Server;

/* Global variables */
Server server;
Controller controllers[1024];
int number_of_controllers = 0;
int socket_udp;
int socket_tcp;
pid_t pid;
bool debug_mode = false;
char *controllers__config_file = "controllers.dat\0";
char *server_config_file = "server.cfg\0";

/* Time variables */
time_t now;
struct tm *local_time;

void read_config_file();

void read_controllers_file();

void udp_socket_init();

void tcp_socket_init();

void service_loop();

void receive_packet_udp();

void *evaluate_packet_udp(void *args);

bool is_authorized(const char *client_name, const char *client_mac_address);

bool is_valid_data(const char *client_name, const char *client_random_number);

Packet_Udp build_packet_udp(unsigned char type, const char *mac, const char *random_number, const char *data);

int get_controller_id(char *name);

void set_controller_state(char *name, char *state);

char *get_controller_random_number(char *name);

void set_controller_random_number(char *name, char *new_random_number);

void keep_in_touch_with_controller(int id);

char *get_string_from_type(unsigned char type);

void *start_command_line(void *args);

void list_function();

void read_parameters(int argc, const char* argv[]);

int kill(pid_t pid, int sig);

int main(int argc, const char *argv[]) {
    pthread_t command_line_thread;
    pthread_attr_t attr;

    read_parameters(argc, argv);
    read_controllers_file();
    read_config_file();

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* Initialize thread to evaluate the packet received */
    if (pthread_create(&command_line_thread, &attr, start_command_line,
                       (void *) NULL) != 0) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Couldn't create process to evaluate the packet received\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
        exit(1);
    }
    if (debug_mode) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: DEBUG => Thread created to manage DB controllers\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    }
    udp_socket_init();
    tcp_socket_init();
    service_loop();
    return 0;
}

void read_config_file() {
    FILE *config_file = fopen(server_config_file, "r");
    char line[30];
    char delim[] = " =\n";
    char *token;

    while (fgets(line, 30, config_file)) {
        token = strtok(line, delim);
        if (token != NULL) {
            if (strcmp(token, "Name") == 0) {
                token = strtok(NULL, delim);
                strcpy(server.name, token);
            } else if (strcmp(token, "MAC") == 0) {
                token = strtok(NULL, delim);
                strcpy(server.mac, token);
            } else if (strcmp(token, "UDP-port") == 0) {
                token = strtok(NULL, delim);
                server.udp_port = atoi(token);
            } else if (strcmp(token, "TCP-port") == 0) {
                token = strtok(NULL, delim);
                server.tcp_port = atoi(token);
            }
        }
    }
    fclose(config_file);
    if (debug_mode) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: DEBUG => Configuration file read\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    }
}

void read_controllers_file() {
    FILE *controllers_file = fopen(controllers__config_file, "r");
    char line[30];
    char delim[] = ",\n";
    char *token;

    while (fgets(line, 30, controllers_file)) {
        token = strtok(line, delim);
        if (token != NULL) {
            strcpy(controllers[number_of_controllers].name, token);
            token = strtok(NULL, delim);
            strcpy(controllers[number_of_controllers].mac, token);
            strcpy(controllers[number_of_controllers].state, "DISCONNECTED");
            strcpy(controllers[number_of_controllers].random_number, "00000000");
            number_of_controllers++;
        }
    }
    fclose(controllers_file);
    if (debug_mode) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: DEBUG => Controllers authorized in system read\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    }
}

void udp_socket_init() {
    struct sockaddr_in addr_serv;

    /* create INET+DGRAM socket -> UDP */
    socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_udp < 0) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Couldn't create UDP socket\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
        exit(1);
    }

    /* fill the structure with the addresses where we will bind the server */
    memset(&addr_serv, 0, sizeof(struct sockaddr_in));
    addr_serv.sin_family = AF_INET;
    addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_serv.sin_port = htons(server.udp_port);

    /* bind */
    if (bind(socket_udp, (struct sockaddr *) &addr_serv, sizeof(struct sockaddr_in)) < 0) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Couldn't bind UDP socket\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
        exit(1);
    }
    if (debug_mode) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: DEBUG => Socket UDP active\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    }
}

void tcp_socket_init() {
    struct sockaddr_in addr_cli;

    /* Create TCP socket */
    socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_tcp < 0) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Couldn't create TCP socket\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
        exit(1);
    }

    /* Fill the structure with the addresses where we will bind the client (any local address) */
    memset(&addr_cli, 0, sizeof(struct sockaddr_in));
    addr_cli.sin_family = AF_INET;
    addr_cli.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_cli.sin_port = htons(server.tcp_port);

    /* bind */
    if (bind(socket_tcp, (struct sockaddr *) &addr_cli, sizeof(struct sockaddr_in)) < 0) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Couldn't bind TCP socket\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
        exit(1);
    }
    if (debug_mode) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: DEBUG => Socket TCP active\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    }

    /* listen */
    if (listen(socket_tcp, 10) < 0) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Couldn't listen TCP socket\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
        exit(1);
    }
}

void service_loop() {
    while (1) {
        receive_packet_udp();
    }
}

void receive_packet_udp() {
    /* Packet data */
    char *buffer = malloc(sizeof(Packet_Udp));
    char *client_ip_address = malloc(INET_ADDRSTRLEN);
    int client_udp_port;
    Packet_Udp *received_packet = (Packet_Udp *) buffer;

    /* Thread */
    pthread_t udp_packet_evaluation_thread;
    pthread_attr_t attr;
    char *args[3];

    /* Prepare the structure to store the client address */
    struct sockaddr_in addr_cli;
    socklen_t addr_cli_len = sizeof(addr_cli);
    memset(&addr_cli, 0, sizeof(struct sockaddr_in));
    memset(buffer, 0, sizeof(Packet_Udp));
    memset(client_ip_address, 0, INET_ADDRSTRLEN);

    /* Receive UDP packet */
    if (recvfrom(socket_udp, (char *) buffer, sizeof(Packet_Udp), 0,
                 (struct sockaddr *) &addr_cli, &addr_cli_len) < 0) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Couldn't receive UDP packet\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
        exit(1);
    }
    received_packet = (Packet_Udp *) buffer;
    if (debug_mode) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: DEBUG => Received: bytes=103, command=%s, mac=%s, rndm=%s, info=%s\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
               get_string_from_type(received_packet->type), received_packet->mac, received_packet->random_number,
               received_packet->data);
    }

    /* Get ip address and port from the client */
    inet_ntop(AF_INET, &(addr_cli.sin_addr), client_ip_address, INET_ADDRSTRLEN);
    client_udp_port = ntohs(addr_cli.sin_port);

    /* Prepare parameters for the thread */
    args[0] = buffer;
    args[1] = client_ip_address;
    args[2] = (char *) &client_udp_port;

    /* Detach the thread */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    /* Initialize thread to evaluate the packet received */
    if (pthread_create(&udp_packet_evaluation_thread, &attr, evaluate_packet_udp,
                       (void *) args) != 0) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Couldn't create process to evaluate the packet received\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
        exit(1);
    }
}

void *evaluate_packet_udp(void *args) {
    /* Set variables */
    int i = 0;
    char *buffer = ((char **) args)[0];
    char *client_ip_address = ((char **) args)[1];
    int client_udp_port = *((int *) ((char **) args)[2]);

    struct sockaddr_in client_addr;
    struct sockaddr_in new_addr_serv;
    socklen_t new_addr_len = sizeof(new_addr_serv);

    Packet_Udp *received_packet = (Packet_Udp *) buffer;
    char *token;
    char name[20];
    char situation[20];

    Packet_Udp packet_to_send;
    char data_to_send[80];

    /* Where will the packet be sent */
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(client_udp_port);
    if (inet_pton(AF_INET, client_ip_address, &(client_addr.sin_addr)) <= 0) {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Invalid address\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
        exit(1);
    }

    if (received_packet->type == 0x00) {
        /* Extract the controller's name and its situation from the data field of the received packet */
        char new_random_number[9];
        token = strtok(received_packet->data, ",");
        if (token != NULL) {
            strcpy(name, token);
            name[strlen(name)] = '\0';
            token = strtok(NULL, ",");
            if (token != NULL) {
                strcpy(situation, token);
                situation[strlen(situation) - 1] = '\0';
            }
        }

        if (!is_authorized(name, received_packet->mac)) {
            packet_to_send = build_packet_udp(0x02, server.mac, "00000000", "The controller is not authorized");

        } else if (!is_valid_data(name, received_packet->random_number)) {
            packet_to_send = build_packet_udp(0x02, server.mac, "00000000", "Data is not valid");

        } else {

            /* Create a new UDP socket */
            int new_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (new_udp_socket < 0) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: ERROR => Server: Couldn't create new UDP socket\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
                exit(1);
            }

            /* Prepare the structure with the addresses where we will bind the new socket */
            memset(&new_addr_serv, 0, sizeof(struct sockaddr_in));
            new_addr_serv.sin_family = AF_INET;
            new_addr_serv.sin_addr.s_addr = htonl(INADDR_ANY);
            new_addr_serv.sin_port = htons(0);

            /* Bind the new socket to the new port */
            if (bind(new_udp_socket, (struct sockaddr *) &new_addr_serv, sizeof(struct sockaddr_in)) < 0) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: ERROR => Server: Couldn't bind new UDP socket\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
                exit(1);
            }

            /* Get the socket's assigned port */
            if (getsockname(new_udp_socket, (struct sockaddr *) &new_addr_serv, &new_addr_len) == -1) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: ERROR => Server Couldn't get the socket's assigned port\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
                exit(1);
            }

            /* Cast to string */
            sprintf(data_to_send, "%d", htons(new_addr_serv.sin_port));

            /* Generate a new random number */
            while (i < 8) { new_random_number[i] = '0' + rand() % 10; i++; }
            new_random_number[i] = '\0';
            set_controller_random_number(name, new_random_number);

            /* Build packet to send */
            packet_to_send = build_packet_udp(0x01, server.mac, new_random_number, data_to_send);

            /* Send the packet */
            if (sendto(socket_udp, &packet_to_send, sizeof(Packet_Udp), 0, (struct sockaddr *) &client_addr,
                       sizeof(client_addr)) < 0) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: ERROR => Server: Couldn't send packet via UDP\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
                exit(1);
            }
            if (debug_mode) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: DEBUG => Sent: bytes=103, command=%s, mac=%s, rndm=%s, info=%s\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
                       get_string_from_type(packet_to_send.type), server.mac,
                       packet_to_send.random_number, packet_to_send.data);
            }

            set_controller_state(name, "WAIT_INFO\0");
            time(&now);
            local_time = localtime(&now);
            printf("%02d:%02d:%02d: MSG.  => Controller: %s state changed to %s\n",
                   local_time->tm_hour, local_time->tm_min, local_time->tm_sec, name,
                   controllers[get_controller_id(name)].state);

            /* Wait for the client send_info packet */
            if (recvfrom(new_udp_socket, (char *) buffer, sizeof(Packet_Udp), 0,
                         (struct sockaddr *) &client_addr, &new_addr_len) < 0) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: ERROR => Server: Couldn't receive UDP packet\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
                exit(1);
            }

            received_packet = (Packet_Udp *) buffer;
            if (debug_mode) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: DEBUG => Received: bytes=103, command=%s, mac=%s, rndm=%s, info=%s\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
                       get_string_from_type(received_packet->type), received_packet->mac,
                       received_packet->random_number,
                       received_packet->data);
            }
            if (received_packet->type == 0x03) {
                if (strcmp(received_packet->mac, controllers[get_controller_id(name)].mac) != 0 ||
                    strcmp(received_packet->random_number, controllers[get_controller_id(name)].random_number) !=
                    0) {
                    packet_to_send = build_packet_udp(0x02, server.mac, new_random_number, data_to_send);
                    set_controller_state(name, "DISCONNECTED");
                    set_controller_random_number(name, "00000000");
                    time(&now);
                    local_time = localtime(&now);
                    printf("%02d:%02d:%02d: MSG.  => Controller: %s state changed to %s\n",
                           local_time->tm_hour, local_time->tm_min, local_time->tm_sec, name,
                           controllers[get_controller_id(name)].state);
                } else {
                    token = strtok(received_packet->data, ",");
                    if (token != NULL) {
                        controllers[get_controller_id(name)].tcp_port = atoi(token);
                        token = strtok(NULL, ",");
                        if (token != NULL) {
                            strcpy(controllers[get_controller_id(name)].devices, token);
                            controllers[get_controller_id(name)].devices[200 - 1] = '\0';
                        }
                    }

                    sprintf(data_to_send, "%d", server.tcp_port);
                    packet_to_send = build_packet_udp(0X04, server.mac, new_random_number, data_to_send);

                    set_controller_state(name, "SUBSCRIBED\0");
                    time(&now);
                    local_time = localtime(&now);
                    printf("%02d:%02d:%02d: MSG.  => Controller: %s state changed to %s\n",
                           local_time->tm_hour, local_time->tm_min, local_time->tm_sec, name,
                           controllers[get_controller_id(name)].state);
                }


            } else {
                packet_to_send = build_packet_udp(0x02, server.mac, "00000000", "Data is not valid");
            }

            if (sendto(socket_udp, &packet_to_send, sizeof(Packet_Udp), 0,
                       (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: ERROR => Server: Couldn't send the packet\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
                exit(1);
            }
            if (debug_mode) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: DEBUG => Sent: bytes=103, command=%s, mac=%s, rndm=%s, info=%s\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
                       get_string_from_type(packet_to_send.type), server.mac,
                       packet_to_send.random_number, packet_to_send.data);
            }

            keep_in_touch_with_controller(get_controller_id(name));
        }

    } else if (received_packet->type == 0x10) {
        strcpy(data_to_send, received_packet->data);
        token = strtok(received_packet->data, ",");
        if (token != NULL) {
            strcpy(name, token);
            name[strlen(name)] = '\0';
            token = strtok(NULL, ",");
            if (token != NULL) {
                strcpy(situation, token);
                situation[strlen(situation) - 1] = '\0';
            }
        }

        if (!is_authorized(name, received_packet->mac)) {
            packet_to_send = build_packet_udp(0x11, server.mac, get_controller_random_number(name),
                                              "The controller is not authorized");

        } else if (!is_valid_data(name, received_packet->random_number)) {
            packet_to_send = build_packet_udp(0x11, server.mac, get_controller_random_number(name),
                                              "Data is not valid");

        } else {
            set_controller_state(name, "SEND_HELLO\0");
            controllers[get_controller_id(name)].hello_received = true;
            packet_to_send = build_packet_udp(0x10, server.mac, get_controller_random_number(name),
                                              data_to_send);
        }

        if (sendto(socket_udp, &packet_to_send, sizeof(Packet_Udp), 0,
                   (struct sockaddr *) &client_addr, sizeof(client_addr)) < 0) {
            time(&now);
            local_time = localtime(&now);
            printf("%02d:%02d:%02d: ERROR => Server: Couldn't send the packet via UDP\n",
                   local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
            exit(1);
        }
        if (debug_mode) {
            time(&now);
            local_time = localtime(&now);
            printf("%02d:%02d:%02d: DEBUG => Sent: bytes=103, command=%s, mac=%s, rndm=%s, info=%s\n",
                   local_time->tm_hour, local_time->tm_min, local_time->tm_sec,
                   get_string_from_type(packet_to_send.type), server.mac,
                   packet_to_send.random_number, packet_to_send.data);
        }

    } else {
        time(&now);
        local_time = localtime(&now);
        printf("%02d:%02d:%02d: ERROR => Server: Received UNKNOWN type of packet\n",
               local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    }

    return NULL;
}

bool is_authorized(const char *client_name, const char *client_mac_address) {
    int i = 0;
    while (i < number_of_controllers) {
        if (strcmp(controllers[i].name, client_name) == 0) {
            if (strcmp(controllers[i].mac, client_mac_address) == 0) {
                return true;
            }
            break;
        }
        i++;
    }
    return false;
}

bool is_valid_data(const char *client_name, const char *client_random_number) {
    int i = 0;
    while (i < number_of_controllers) {
        if (strcmp(controllers[i].name, client_name) == 0) {
            if (strcmp(controllers[i].random_number, client_random_number) == 0) {
                return true;
            }
            break;
        }
        i++;
    }
    return false;
}

Packet_Udp build_packet_udp(unsigned char type, const char *mac, const char *random_number, const char *data) {
    Packet_Udp packet;
    packet.type = type;
    strcpy(packet.mac, mac);
    strcpy(packet.random_number, random_number);
    strcpy(packet.data, data);
    return packet;
}

int get_controller_id(char *name) {
    int i = 0;
    while (i < number_of_controllers) {
        if (strcmp(controllers[i].name, name) == 0) {
            return i;
        }
        i++;
    }
    return -1;
}

char *get_controller_state(char *name) {
    int i = 0;
    while (i < number_of_controllers) {
        if (strcmp(controllers[i].name, name) == 0) {
            return controllers[i].state;
        }
        i++;
    }
    return "UNKNOWN";
}

void set_controller_state(char *name, char *state) {
    int i = 0;
    while (i < number_of_controllers) {
        if (strcmp(controllers[i].name, name) == 0) {
            strcpy(controllers[i].state, state);
            break;
        }
        i++;
    }
}

char *get_controller_random_number(char *name) {
    int i = 0;
    while (i < number_of_controllers) {
        if (strcmp(controllers[i].name, name) == 0) {
            return controllers[i].random_number;
        }
        i++;
    }
    return "00000000";
}

void set_controller_random_number(char *name, char *new_random_number) {
    int i = 0;
    while (i < number_of_controllers) {
        if (strcmp(controllers[i].name, name) == 0) {
            strcpy(controllers[i].random_number, new_random_number);
            break;
        }
        i++;
    }
}

void keep_in_touch_with_controller(int id) {
    time_t hello_timeout;
    bool hello_packet_received;

    while (1) {
        hello_packet_received = false;

        if (strcmp(controllers[id].state, "SEND_HELLO") == 0) {
            hello_timeout = time(NULL) + V;
            while (time(NULL) < hello_timeout) {
                if (controllers[id].hello_received) {
                    hello_packet_received = true;
                    controllers[id].consecutive_lost_packets = 0;
                    controllers[id].hello_received = false;
                }
            }

            if (!hello_packet_received) {
                controllers[id].consecutive_lost_packets += 1;
                if (controllers[id].consecutive_lost_packets == X) {
                    time(&now);
                    local_time = localtime(&now);
                    printf("%02d:%02d:%02d: MSG.  => Controller %s [%s] has not received %d consecutive HELLO\n",
                           local_time->tm_hour, local_time->tm_min, local_time->tm_sec, controllers[id].name,
                           controllers[id].mac, X);
                    set_controller_state(controllers[id].name, "DISCONNECTED");
                    set_controller_random_number(controllers[id].name, "00000000");
                    time(&now);
                    local_time = localtime(&now);
                    printf("%02d:%02d:%02d: MSG.  => Controller: %s state changes to %s\n",
                           local_time->tm_hour, local_time->tm_min, local_time->tm_sec, controllers[id].name,
                           controllers[id].state);
                    controllers[id].consecutive_lost_packets = 0;
                    return;
                }
            }

        } else if (strcmp(controllers[id].state, "SUBSCRIBED") == 0) {
            hello_timeout = time(NULL) + V * R;
            while (time(NULL) < hello_timeout) {
                if (controllers[id].hello_received) {
                    time(&now);
                    local_time = localtime(&now);
                    printf("%02d:%02d:%02d: MSG.  => Controller %s: state changes to %s\n",
                           local_time->tm_hour, local_time->tm_min, local_time->tm_sec, controllers[id].name,
                           controllers[id].state);
                    hello_packet_received = true;
                    controllers[id].consecutive_lost_packets = 0;
                    controllers[id].hello_received = false;
                }
            }

            if (!hello_packet_received) {
                time(&now);
                local_time = localtime(&now);
                printf("%02d:%02d:%02d: MSG.  => Controller %s: state changes to %s\n",
                       local_time->tm_hour, local_time->tm_min, local_time->tm_sec, controllers[id].name,
                       controllers[id].state);
                set_controller_state(controllers[id].name, "DISCONNECTED");
                set_controller_random_number(controllers[id].name, "00000000");
                controllers[id].consecutive_lost_packets = 0;
                return;
            }
        } else {
            time(&now);
            local_time = localtime(&now);
            printf("%02d:%02d:%02d: ERROR => Controller %s: Invalid controller state\n",
                   local_time->tm_hour, local_time->tm_min, local_time->tm_sec, controllers[id].name);
            return;
        }
    }
}

void *start_command_line(void *args) {
    char input[100];

    /* Read input in a loop until the user enters "exit" */
    while (1) {
        scanf("%s", input);

        /* Check if the user entered "exit" */
        if (strcmp(input, "quit") == 0) {
            kill(pid, SIGKILL);
        } else if (strcmp(input, "list") == 0) {
            list_function();
        } else {
            printf("Unknown command\n");
        }
    }

    return NULL;
}

void list_function() {
    int i = 0;
    printf("--NAME-- ------IP------- -----MAC---- --RNDM--- ----STATE--- --SITUATION-- --ELEMENTS-------------------------------------------\n");
    while (i < number_of_controllers) {
        printf("%-8s %-15s %-12s %-8s %-12s %-12s %-60s\n", controllers[i].name, controllers[i].ip, controllers[i].mac,
               controllers[i].random_number, controllers[i].state, controllers[i].situation, controllers[i].devices);
        i++;
    }
}

void read_parameters(int argc, const char* argv[]) {

    int i = 1;
    if (argc >= 2) {
        while (i < argc) {
            if (strcmp(argv[i], "-d") == 0) {
                debug_mode = true;
            } else if (strcmp(argv[i], "-c") == 0) {
                if (i + 1 < argc) {
                    strcpy(controllers__config_file, argv[i + 1]);
                    i++;
                }
            } else if (strcmp(argv[i], "-u") == 0) {
                if (i + 1 < argc) {
                    strcpy(server_config_file, argv[i + 1]);
                    i++;
                }
            }
            i++;
        }
    }
}

char *get_string_from_type(unsigned char type) {
    if (type == 0x00) return "SUBS_REQ";
    else if (type == 0x01) return "SUBS_ACK";
    else if (type == 0x02) return "SUBS_REJ";
    else if (type == 0x03) return "SUBS_INFO";
    else if (type == 0x04) return "INFO_ACK";
    else if (type == 0x05) return "SUBS_NACK";
    else if (type == 0x10) return "HELLO";
    else if (type == 0x11) return "HELLO_REJ";
    else if (type == 0x20) return "SEND_DATA";
    else if (type == 0x21) return "SET_DATA";
    else if (type == 0x22) return "GET_DATA";
    else if (type == 0x23) return "DATA_ACK";
    else if (type == 0x24) return "DATA_NACK";
    else if (type == 0x25) return "DATA_REJ";
    else return "UNKNOWN";
}
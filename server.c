#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <signal.h>

/* ------------------------------------------------ GLOBAL VARIABLES ------------------------------------------------ */
/* DICTIONARY */
char *packet_dictionary(unsigned char packet_type) {
    if (packet_type == 0x00) return "SUBS_REQ";
    if (packet_type == 0x01) return "SUBS_ACK";
    if (packet_type == 0x02) return "SUBS_REJ";
    if (packet_type == 0x03) return "SUBS_INFO";
    if (packet_type == 0x04) return "INFO_ACK";
    if (packet_type == 0x05) return "SUBS_NACK";
    if (packet_type == 0x10) return "HELLO";
    if (packet_type == 0x11) return "HELLO_REJ";
    if (packet_type == 0x20) return "SEND_DATA";
    if (packet_type == 0x21) return "SET_DATA";
    if (packet_type == 0x22) return "GET_DATA";
    if (packet_type == 0x23) return "DATA_ACK";
    if (packet_type == 0x24) return "DATA_NACK";
    if (packet_type == 0x25) return "DATA_REJ";
    return "UNKNOWN";
}

/* STRUCTURES */
/* Store information from server.cfg (previously read in read_config()) into config_data structure */
typedef struct {
    char Name[10];
    char MAC[20];
    int UDP_port;
    int TCP_port;
} ConfigData;
ConfigData config_data;

/* Store information from server.cfg (previously read in read_config()) into config_data structure */
typedef struct {
    char Name[10];
    char MAC[20];
    char State[15];
    char IP[30];
    char Random[30];
    char Situation[30];
    char Elements[30];
    int n_elems;
} ControllersData;
ControllersData controllers_data[10];

typedef struct {
    unsigned char type;
    char mac[13];
    char random[9];
    char data[80];
} Packet;
int buffer_size = 1 + 13 + 9 + 80;

typedef struct {
    Packet packet;
    struct sockaddr_in client_addr;
    char* host;
    char* client_port;
} Args;

pid_t pid;

/* SOCKETS */
int sock_udp;
int sock_tcp;

/* ENTRY PARAMETERS */
bool debug = false;
char *config_file = "server.cfg";
char *controllers_file = "controllers.dat";

/* TIMERS */
int s = 2;
int init = 2;

/* THREADS */
pthread_t console;

/* ----------------------------------------------------------------------------------------------------------------- */

void print_msg_time(char *msg) {
    time_t now = time(NULL);
    struct tm tm = *localtime(&now);
    printf("%02d:%02d:%02d: %s", tm.tm_hour, tm.tm_min, tm.tm_sec, msg);
}

char** get_controllers_info(char *data) {
    char **controllers_info = malloc(2 * sizeof(char*));
    controllers_info[0] = strtok(data, ",");  /* Name */
    controllers_info[1] = strtok(NULL, "\n"); /* Situation */
    return controllers_info;
}

int verify_client_id(Packet rcv_packet) {
    int i = 0; int pos = -1;
    char** controllers_info = get_controllers_info(rcv_packet.data);
    char* rcv_packet_name = controllers_info[0];
    char* rcv_packet_situation = controllers_info[1];

    while (i < controllers_data->n_elems) {
        if (strcmp(controllers_data[i].MAC, rcv_packet.mac) == 0) pos = i;
        i++;
    }

    if (pos >= 0) {
        if (rcv_packet.type == 0x00 && strcmp(controllers_data[pos].Name, rcv_packet_name) == 0 &&
        strcmp("00000000", rcv_packet.random) == 0 && strcmp("", rcv_packet_situation) != 0) return pos;
    }
    return -1;
}

char* random_number(int seed) {
    int i = 0; char random_aux[9];
    char *random = (char*) malloc(sizeof(random_aux));
    while (i < 8) {
        random[i] = '0' + (seed + rand() % 8);
        i++;
    }
    random[i] = '\0';
    return random;
}

void read_entry_parameters(int argc, char *argv[]) {
    if (argc >= 2) {
        if (strcmp(argv[1], "-d") == 0) {
            debug = true;
            print_msg_time("DEBUG => Llegits paràmetres línia de comandes\n");
        }
        else if (strcmp(argv[1], "-c") == 0) {
            if (argc >= 3) { config_file = argv[2]; }
            else { print_msg_time("ERROR => No es pot obrir l'arxiu de configuració: \n"); }
        } else if (strcmp(argv[1], "-u") == 0) {
            if (argc >= 3) { controllers_file = argv[2]; }
            else { print_msg_time("ERROR => No es pot obrir l'arxiu dels controladors: \n"); }
        }
    }
}

void list_controllers(void) {
    int i = 0;
    printf("--NOM--- ------IP------ -----MAC---- --RNDM-- ----ESTAT--- --SITUACIÓ-- --ELEMENTS------------------------------------------\n");
    while (controllers_data->n_elems > i) {
        printf("%s              - %s          %s\n", controllers_data[i].Name, controllers_data[i].MAC, controllers_data[i].State);
        i++;
    }
}

/* Read file 'controllers.dat' and save the information (Name, MAC, State, IP, Random, Situation, Elements) inside controllers_data.*/
void read_controllers(void) {
    FILE *file = fopen(controllers_file, "r");
    char line[25];
    int nline = 0;

    while (fgets(line, sizeof(line), file)) {
        char *name = strtok(line, ",");
        char *mac = strtok(NULL, "\n");

        strcpy(controllers_data[nline].Name, name);
        strcpy(controllers_data[nline].MAC, mac);
        strcpy(controllers_data[nline].State, "DISCONNECTED");
        nline++;
    }
    controllers_data->n_elems = nline;
    fclose(file);

    if (debug) {
        print_msg_time("DEBUG => Llegits "); printf("%i autoritats en el sistema\n", controllers_data->n_elems);
        list_controllers();
    }
}

/* Read file 'server.cfg' and save the information (Name, MAC, UDP-port, TCP-port) inside config_data.*/
void read_config(void) {
    FILE *file = fopen(config_file, "r");
    char line[25];

    while (fgets(line, sizeof(line), file)) {
        char *key = strtok(line, " =");
        char *val = strtok(NULL, " =\n");

        if (strcmp(key, "Name") == 0) { strcpy(config_data.Name, val); } else if (
            strcmp(key, "MAC") == 0) { strcpy(config_data.MAC, val); } else if (strcmp(key, "UDP-port") == 0) {
                config_data.UDP_port = atoi(val);
            } else if (strcmp(key, "TCP-port") == 0) { config_data.TCP_port = atoi(val); }
    }
    fclose(file);
    if (debug) { print_msg_time("DEBUG => Llegits paràmetres arxiu de configuració\n"); }
}

void *system_info(void *args) {
    char input[100];

    while (true) {
        scanf("%s", input);
        if (strcmp(input, "list") == 0) list_controllers();
        else if (strcmp(input, "set") == 0) printf("Sending information to an entry element of the controller...\n");
        else if (strcmp(input, "get") == 0) printf("Requesting information of the element of the controller...\n");
        else if (strcmp(input, "quit") == 0) kill(pid, SIGKILL);
        else {
            printf("Unknown command. Please enter one of the following commands:\n");
            printf("- list\n- set <controller_name> <device_name> <value>\n- get <controller_name> <device_name>\n- quit");
        }
    }
}

void udp_socket(void) {
    struct sockaddr_in addr;
    sock_udp = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_data.UDP_port);

    bind(sock_udp, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
}

void tcp_socket(void) {
    struct sockaddr_in addr;
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_data.TCP_port);

    bind(sock_tcp, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    listen(sock_tcp, 5);
}

void *classify_packet();

void read_packet(void) {
    char *client_host = malloc(INET_ADDRSTRLEN);
    int client_port;
    Packet rcv_packet;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t thread;
    Args arg;

    while (true) {
        recvfrom(sock_udp, &rcv_packet, sizeof(Packet), 0, (struct sockaddr *) &client_addr, &addr_len);

        if (debug) {
            print_msg_time("DEBUG => Rebut: "); printf("bytes=%i, comanda=%s, mac=%s, rndm=%s, dades=%s\n",
            buffer_size, packet_dictionary(rcv_packet.type), rcv_packet.mac, rcv_packet.random, rcv_packet.data);
        }
        inet_ntop(AF_INET, &client_addr.sin_addr, client_host, INET_ADDRSTRLEN);
        client_port = ntohs(client_addr.sin_port);

        arg.packet = rcv_packet; arg.host = client_host; arg.client_port = (char *) &client_port; arg.client_addr = client_addr;
        pthread_create(&thread, NULL, classify_packet, &arg);
        if (debug) print_msg_time("DEBUG => Rebut paquet UDP, creat procés per atendre'l\n");
    }
}

void subs_request(Args arg, int pos);

void *classify_packet(Args *args) {
    struct sockaddr_in client_addr = args->client_addr;
    Packet rcv_packet = args->packet;
    char *host = args->host;
    int port = *(int *) args->client_port;
    int pos = verify_client_id(rcv_packet);

    if (pos >= 0 && strcmp(packet_dictionary(rcv_packet.type), "SUBS_REQ") == 0) {
        printf("PAQUET CLASSIFICAT.\n");
        subs_request(*args, pos);
    }
    return NULL;
}

struct sockaddr_in open_udp_port(void) {
    struct sockaddr_in new_addr_srv;
    socklen_t new_addr_len = sizeof(new_addr_srv);
    int new_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&new_addr_srv, 0, sizeof(struct sockaddr_in));
    new_addr_srv.sin_family = AF_INET;
    new_addr_srv.sin_addr.s_addr = htonl(INADDR_ANY);
    new_addr_srv.sin_port = htons(0);

    bind(new_udp_socket, (struct sockaddr *) &new_addr_srv, sizeof(struct sockaddr_in));
    getsockname(new_udp_socket, (struct sockaddr *) &new_addr_srv, &new_addr_len);
    printf("port obert\n");
    return new_addr_srv;
}

void subs_request(Args arg, int pos) {
    /* ----- VARIABLES ----- */
    Packet packet_to_send;
    Packet rcv_packet = arg.packet;
    int client_port = *((int *) arg.client_port);
    char *client_host = arg.host;
    char *random = random_number(2);
    char data_to_send[80];
    int port;

    struct sockaddr_in client_addr;
    struct sockaddr_in new_addr_srv;
    socklen_t new_addr_len = sizeof(new_addr_srv);
    int new_udp_socket;

    char *buffer = malloc(buffer_size);
    fd_set fds;
    struct timeval tv;
    int timeout;
    /* --------------------- */

    /* Where will the packet be sent */
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(client_port);
    inet_pton(AF_INET, client_host, &client_addr.sin_port);

    /* Open new UDP port */
    new_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&new_addr_srv, 0, sizeof(struct sockaddr_in));
    new_addr_srv.sin_family = AF_INET;
    new_addr_srv.sin_addr.s_addr = htonl(INADDR_ANY);
    new_addr_srv.sin_port = htons(0);

    bind(new_udp_socket, (struct sockaddr *) &new_addr_srv, sizeof(struct sockaddr_in));
    getsockname(new_udp_socket, (struct sockaddr *) &new_addr_srv, &new_addr_len);
    printf("port obert\n");

    /* Wait for SUBS_INFO packet */
    tv.tv_sec = s * init;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(new_udp_socket, &fds);

    /* Build packet to send */
    port = htons(new_addr_srv.sin_port);
    sprintf(data_to_send, "%d", port);
    packet_to_send.type = 0x01;
    strcpy(packet_to_send.mac, config_data.MAC);
    strcpy(packet_to_send.random, random);
    strcpy(packet_to_send.data, data_to_send);

    /* Send the packet */
    sendto(sock_udp, &packet_to_send, sizeof(Packet), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    if (debug) {
        print_msg_time("DEBUG => Enviat: "); printf("bytes=%i, comanda=%s, mac=%s, rndm=%s, dades=%s\n",
        buffer_size, packet_dictionary(packet_to_send.type), packet_to_send.mac, packet_to_send.random, packet_to_send.data);
    }
    strcpy(controllers_data[pos].State, "WAIT_INFO");
    print_msg_time("MSG.  => Controlador passa a l'estat "); printf("%s\n", controllers_data[pos].State);

}

int main(int argc, char *argv[]) {
    read_entry_parameters(argc, argv);
    read_controllers();
    read_config();
    pthread_create(&console, NULL, system_info, NULL);
    udp_socket(); if (debug) { print_msg_time("DEBUG => Socket UDP actiu\n"); }
    tcp_socket(); if (debug) { print_msg_time("DEBUG => Socket TCP actiu\n"); }
    read_packet();

    return 0;
}

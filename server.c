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
#include <unistd.h>

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

/* SOCKETS */
int sock_udp;
int sock_tcp;


/* ENTRY PARAMETERS */
bool debug = true;
char *file_name = "server.cfg";

/* ----------------------------------------------------------------------------------------------------------------- */

void print_msg_time(char *msg) {
    time_t now = time(NULL);
    struct tm tm = *localtime(&now);
    printf("%02d:%02d:%02d: %s", tm.tm_hour, tm.tm_min, tm.tm_sec, msg);
}

char** get_controllers_info(char *data) {
    char **controllers_info = malloc(2 * sizeof(char*));
    controllers_info[0] = strtok(data, ",");
    controllers_info[1] = strtok(NULL, "\n");
    return controllers_info;
}

int verify_client_id(Packet rcv_packet) {
    int i = 0;
    char** controllers_info = get_controllers_info(rcv_packet.data);
    char* rcv_packet_name = controllers_info[0];
    char* rcv_packet_situation = controllers_info[1];

    while (i < controllers_data->n_elems) {
        if (strcmp(controllers_data[i].MAC, rcv_packet.mac) == 0 && strcmp(controllers_data[i].Name, rcv_packet_name) == 0) {
            if (strcmp("00000000", rcv_packet.random) == 0 && strcmp("", rcv_packet_situation) != 0) return i;
        } i++;
    }
    return -1;
}

char* random_number(int seed) {
    int i = 0; char random_aux[9];
    char *random = malloc(sizeof(random_aux));

    while (i < 8) {
        random[i] = '0' + (seed + rand() % 10);
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
            if (argc >= 3) { file_name = argv[2]; }
            else { print_msg_time("ERROR => No es pot obrir l'arxiu de configuració: \n"); }
        }
    }
}

/* Read file 'controllers.dat' and save the information (Name, MAC, State, IP, Random, Situation, Elements) inside controllers_data.*/
void read_controllers() {
    FILE *file = fopen("controllers.dat", "r");
    char line[25];
    int nline = 0;
    int i = 0;

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

    printf("Number of controllers: %i\n", controllers_data->n_elems);
    if (debug) {
        print_msg_time("DEBUG => Llegits 6 autoritzats en el sistema\n");
        printf("--NOM--- ------IP------ -----MAC---- --RNDM-- ----ESTAT--- --SITUACIÓ-- --ELEMENTS------------------------------------------\n");
        while (nline > i) {
            printf("%s              - %s          %s\n", controllers_data[i].Name, controllers_data[i].MAC, controllers_data[i].State);
            i++;
        }
    }
}

/* Read file 'server.cfg' and save the information (Name, MAC, UDP-port, TCP-port) inside config_data.*/
void read_config() {
    FILE *file = fopen(file_name, "r");
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

void udp_socket() {
    struct sockaddr_in addr;
    sock_udp = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_data.UDP_port);

    bind(sock_udp, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
}

void tcp_socket() {
    struct sockaddr_in addr;
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_data.TCP_port);

    bind(sock_tcp, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    listen(sock_tcp, 5);
}

void *classify_packet(void *arg);

void read_packet() {
    char *buffer = malloc(buffer_size);
    char *client_host = malloc(INET_ADDRSTRLEN);
    int client_port;
    Packet *rcv_packet;

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t thread;
    char *arg[3];

    recvfrom(sock_udp, buffer, sizeof(Packet), 0, (struct sockaddr *) &client_addr, &addr_len);
    rcv_packet = (Packet *) buffer;
    print_msg_time("DEBUG => Rebut: ");printf("bytes=%i, comanda=%s, mac=%s, rndm=%s, dades=%s\n",
        buffer_size, packet_dictionary(rcv_packet->type), rcv_packet->mac, rcv_packet->random, rcv_packet->data);

    inet_ntop(AF_INET, &client_addr.sin_addr, client_host, INET_ADDRSTRLEN);
    client_port = ntohs(client_addr.sin_port);

    arg[0] = buffer; arg[1] = client_host; arg[2] = (char *) &client_port;
    pthread_create(&thread, NULL, classify_packet, arg);
    print_msg_time("DEBUG => Rebut paquet UDP, creat procés per atendre'l\n");
    sleep(300);
}

void send_packet();
void subs_request();

void *classify_packet(void *arg) {
    Packet *rcv_packet = (Packet *) ((char **) arg)[0];
    char *host = ((char **) arg)[1];
    int port = *(int*) ((char **) arg)[2];
    int pos = verify_client_id(*rcv_packet);

    if (strcmp(packet_dictionary(rcv_packet->type), "SUBS_REQ") == 0) {
        if (pos >= 0 && strcmp(controllers_data[pos].State, "DISCONNECTED") == 0) {
            subs_request(arg, pos);
        } else {
            send_packet(rcv_packet, host, port, "SUBS_REJ", pos);
        }
    }
    return NULL;
}

Packet build_packet(Packet rcv_packet, char* packet_type) {
    Packet packet;
    if (strcmp(packet_type, "SUBS_REJ") == 0) {
        packet.type = 0x02;
        strcpy(packet.mac, rcv_packet.mac); strcpy(packet.random, "00000000");
        strcpy(packet.data, "Dades d'identificació incorrectes");
    }
    return packet;
}

void send_packet(Packet rcv_packet, char* host, int port, char* packet_type, int pos) {
    Packet packet = build_packet(rcv_packet, packet_type);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    sendto(sock_udp, &packet, sizeof(Packet), 0, (struct sockaddr *) &addr, sizeof(addr));
    if (strcmp(packet_type, "SUBS_REJ") == 0) { strcpy(controllers_data[pos].State, "DISCONNECTED"); }
}

void send_new_packet(Packet packet, int port, char* host, int pos);

void subs_request(void* arg, int pos) {
    Packet packet;
    Packet *rcv_packet = (Packet *) ((char **) arg)[0];
    char *host = ((char **) arg)[1];
    int client_port = *(int*) ((char **) arg)[2];
    char* random = random_number(2);

    /* Open UDP port */
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int open_sock = socket(AF_INET, SOCK_DGRAM, 0);
    int port;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);
    bind(open_sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    getsockname(open_sock, (struct sockaddr *) &addr, &addr_len);
    /**/

    packet.type = 0x01; strcpy(packet.mac, rcv_packet->mac); strcpy(packet.random, random);
    port = htons(addr.sin_port); sprintf(packet.data, "%i", port);
    send_new_packet(packet, client_port, host, pos);
}

void send_new_packet(Packet packet, int port, char* host, int pos) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    sendto(sock_udp, &packet, sizeof(Packet), 0, (struct sockaddr *) &addr, sizeof(addr));
    if (packet.type == 0x01) { strcpy(controllers_data[pos].State, "WAIT_INFO"); }
}


int main(int argc, char *argv[]) {
    read_entry_parameters(argc, argv);
    read_controllers();
    read_config();
    udp_socket(); if (debug) { print_msg_time("DEBUG => Socket UDP actiu\n"); }
    tcp_socket(); if (debug) { print_msg_time("DEBUG => Socket TCP actiu\n"); }
    read_packet();

    return 0;
}

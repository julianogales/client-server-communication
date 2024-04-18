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
} ControllersData;
ControllersData controllers_data[10];

typedef struct {
    unsigned char type;
    char mac[13];
    char random[9];
    char data[80];
    int size = 1 + 13 + 9 + 80;
} Packet;


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
    fclose(file);

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
    if (debug) { print_msg_time("DEBUG => Socket UDP actiu\n"); }
}

void tcp_socket() {
    struct sockaddr_in addr;
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_data.TCP_port);

    bind(sock_tcp, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    listen(sock_tcp, 5);
    if (debug) { print_msg_time("DEBUG => Socket TCP actiu\n"); }
}

void receive_packets() {
    char *buffer = malloc(sizeof(Packet));
    char *client_host = malloc(INET_ADDRSTRLEN);
    char *client_port;
    Packet *rcv_packet;
    struct sockaddr_in client_addr;

    recvfrom(sock_udp, buffer, sizeof(Packet), 0, &client_addr, (socklen_t *) sizeof(&client_addr));
    rcv_packet = (Packet *) buffer;
    print_msg_time("DEBUG => Rebut: ");printf("bytes=%i, comanda=%s, mac=%s, rndm=%s, dades=%s",
        rcv_packet->size, packet_dictionary(rcv_packet->type), rcv_packet->mac, rcv_packet->random, rcv_packet->data);

    inet_ntop(AF_INET, &client_addr.sin_addr, client_host, INET_ADDRSTRLEN);

}

void subscription_request() {

}

int main(int argc, char *argv[]) {
    read_entry_parameters(argc, argv);
    read_controllers();
    read_config();
    udp_socket();
    tcp_socket();
    receive_packets();

    subscription_request();
    return 0;
}

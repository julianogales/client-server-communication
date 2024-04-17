#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ------------------------------------------------ GLOBAL VARIABLES ------------------------------------------------ */
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


/* SOCKET */
int sock_udp;
int sock_tcp;

/* ----------------------------------------------------------------------------------------------------------------- */


/* Read file 'server.cfg' and save the information (Name, MAC, UDP-port, TCP-port) inside config_data.*/
void read_config() {
    FILE *file = fopen("server.cfg", "r");
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
}

/* Read file 'controllers.dat' and save the information (Name, MAC, State, IP, Random, Situation, Elements) inside controllers_data.*/
void read_controllers() {
    FILE *file = fopen("controllers.dat", "r");
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
    fclose(file);
}

void udp_socket() {
    sock_udp = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_data.UDP_port);

    bind(sock_udp, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
}

void tcp_socket() {
    sock_tcp = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(config_data.TCP_port);

    bind(sock_tcp, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

    listen(sock_tcp, 5);
}

void subscription_request() {

}

int main() {
    read_config();
    read_controllers();
    udp_socket();
    tcp_socket();

    subscription_request();
    return 0;
}

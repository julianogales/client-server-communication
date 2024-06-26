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
    char Elements[60];
    int TCP_port;
    int n_elems;
    bool hello;
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
int v = 2;
int r = 2;
int x = 3;
int init = 2;

/* THREADS */
pthread_t console;

/* ------------------------------------------------------------------------------------------------------------------ */

/* ------------------------------------------------ DECLARED METHODS ------------------------------------------------ */
/* Auxiliar */
void print_msg_time(char *msg);
void list_controllers(void);
char** get_controllers_info(char *data);
int verify_client_id(Packet rcv_packet, struct sockaddr_in client_addr);
void *classify_packet(Args *args);
char* random_number(int seed);
void send_hello_rej(Packet rcv_packet, int pos, struct sockaddr_in client_addr);

/* Main */
void read_entry_parameters(int argc, char *argv[]);
void read_controllers(void);
void read_config(void);
void *system_input(void *args);
void udp_socket(void);
void tcp_socket(void);
void subs_request(Args arg, int pos);
void count_hello(int pos);
void communication(Args arg, int pos);
/* ------------------------------------------------------------------------------------------------------------------ */

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

int verify_client_id(Packet rcv_packet, struct sockaddr_in client_addr) {
    int i = 0; int pos = -1;
    char** controllers_info = get_controllers_info(rcv_packet.data);
    char* rcv_packet_name = controllers_info[0];
    char* rcv_packet_situation = controllers_info[1];

    while (i < controllers_data->n_elems && pos < 0) {
        if (strcmp(controllers_data[i].MAC, rcv_packet.mac) == 0) pos = i;
        i++;
    }

    if (pos >= 0) {
        /* rcv_packet = SUBS_REQ */
        if (rcv_packet.type == 0x00 && strcmp(controllers_data[pos].Name, rcv_packet_name) == 0 &&
            strcmp("00000000", rcv_packet.random) == 0 && strcmp("", rcv_packet_situation) != 0) {
            strcpy(controllers_data[pos].Situation, rcv_packet_situation);
            return pos;
        }
        /* rcv_packet = SUBS_INFO */
        else if (rcv_packet.type == 0x03 && strcmp(controllers_data[pos].Random, rcv_packet.random) == 0) return pos;

        /* rcv_packet = first HELLO || following HELLO */
        else if (rcv_packet.type == 0x10 && (strcmp(controllers_data[pos].State, "SUBSCRIBED") == 0 ||
        strcmp(controllers_data[pos].State, "SEND_HELLO") == 0)) {
            if (strcmp(controllers_data[pos].Random, rcv_packet.random) == 0 && strcmp(controllers_data[pos].Name, rcv_packet_name) == 0
            && strcmp(controllers_data[pos].Situation, rcv_packet_situation) == 0) return pos;
            else send_hello_rej(rcv_packet, pos, client_addr);
        }
    }
    return -1;
}

char* random_number(int seed) {
    int i = 0; char random_aux[9];
    char *random = (char*) malloc(sizeof(random_aux));
    while (i < 8) {
        random[i] = '0' + ((seed + rand()) % 10);
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
        printf("%-8s %-14s %-12s %-8s %-12s %-12s %s\n", controllers_data[i].Name, controllers_data[i].IP, controllers_data[i].MAC,
               controllers_data[i].Random, controllers_data[i].State, controllers_data[i].Situation, controllers_data[i].Elements);
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
        print_msg_time("DEBUG => Llegits "); printf("%i equips autoritzats en el sistema\n", controllers_data->n_elems);
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

void *system_input(void *args) {
    char input[100];

    while (true) {
        scanf("%s", input);
        if (strcmp(input, "list") == 0) list_controllers();
        else if (strcmp(input, "set") == 0) printf("\n");
        else if (strcmp(input, "get") == 0) printf("\n");
        else if (strcmp(input, "quit") == 0) {
            if(debug) print_msg_time("DEBUG => Petició de finalització\n");
            kill(pid, SIGKILL);
        }
        else {
            print_msg_time("MSG.  => Comanda incorrecta "); printf("(%s)\n", input);
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
        pthread_create(&thread, NULL, (void *(*)(void *)) classify_packet, &arg);
        if (debug) print_msg_time("DEBUG => Rebut paquet UDP, creat procés per atendre'l\n");
    }
}

void *classify_packet(Args *args) {
    Packet rcv_packet = args->packet;
    struct sockaddr_in client_addr = args->client_addr;
    int pos = verify_client_id(rcv_packet, client_addr);

    if (pos >= 0 && strcmp(packet_dictionary(rcv_packet.type), "SUBS_REQ") == 0) {
        subs_request(*args, pos);
    } else if ( pos >= 0 && strcmp(packet_dictionary(rcv_packet.type), "HELLO") == 0) {
        communication(*args, pos);
    }
    return NULL;
}

void subs_request(Args arg, int pos) {
    /* ----- VARIABLES ----- */
    Packet packet_to_send;
    Packet rcv_packet = arg.packet;
    char *random = random_number(2);
    char data_to_send[80];
    int port;
    char client_ip[INET_ADDRSTRLEN];

    struct sockaddr_in client_addr = arg.client_addr;
    struct sockaddr_in new_addr_srv;
    socklen_t new_addr_len = sizeof(new_addr_srv);
    int new_udp_socket;

    char *buffer = malloc(buffer_size);
    fd_set fds;
    struct timeval tv;
    int timeout;
    /* --------------------- */

    /* Open new UDP port */
    new_udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&new_addr_srv, 0, sizeof(struct sockaddr_in));
    new_addr_srv.sin_family = AF_INET;
    new_addr_srv.sin_addr.s_addr = htonl(INADDR_ANY);
    new_addr_srv.sin_port = htons(0);
    inet_ntop(AF_INET, &(arg.client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);

    bind(new_udp_socket, (struct sockaddr *) &new_addr_srv, sizeof(struct sockaddr_in));
    getsockname(new_udp_socket, (struct sockaddr *) &new_addr_srv, &new_addr_len);

    /* Build packet to send */
    port = htons(new_addr_srv.sin_port);
    sprintf(data_to_send, "%d", port);
    packet_to_send.type = 0x01;
    strcpy(packet_to_send.mac, config_data.MAC);
    strcpy(packet_to_send.random, random);
    strcpy(packet_to_send.data, data_to_send);

    /* Update random in controllers_list */
    strcpy(controllers_data[pos].Random, random);

    /* Send the packet */
    sendto(sock_udp, &packet_to_send, sizeof(Packet), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    if (debug) {
        print_msg_time("DEBUG => Enviat: "); printf("bytes=%i, comanda=%s, mac=%s, rndm=%s, dades=%s\n",
        buffer_size, packet_dictionary(packet_to_send.type), packet_to_send.mac, packet_to_send.random, packet_to_send.data);
    }
    strcpy(controllers_data[pos].State, "WAIT_INFO");
    print_msg_time("MSG.  => Controlador: "); printf("%s, passa a l'estat: %s\n", controllers_data[pos].Name, controllers_data[pos].State);


    /* Wait for SUBS_INFO packet */
    tv.tv_sec = s * init;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(new_udp_socket, &fds);

    timeout = select(new_udp_socket+1, &fds, NULL, NULL, &tv);
    if (timeout > 0) {
        int new_pos;
        recvfrom(new_udp_socket, buffer, sizeof(Packet), 0, (struct sockaddr *) &client_addr, &new_addr_len);
        rcv_packet = *(Packet *) buffer;
        if (debug) {
            print_msg_time("DEBUG => Rebut: "); printf("bytes=%i, comanda=%s, mac=%s, rndm=%s, dades=%s\n",
            buffer_size, packet_dictionary(rcv_packet.type), rcv_packet.mac, rcv_packet.random, rcv_packet.data);
        }

        new_pos = verify_client_id(rcv_packet, client_addr);
        if (strcmp(packet_dictionary(rcv_packet.type), "SUBS_INFO") == 0 && new_pos >= 0) {
            char** controllersInfo = get_controllers_info(rcv_packet.data);
            controllers_data[new_pos].TCP_port = atoi(controllersInfo[0]);
            strcpy(controllers_data[new_pos].Elements, controllersInfo[1]);
            strcpy(controllers_data[new_pos].IP, client_ip);

            /* Build INFO_ACK packet */
            sprintf(data_to_send, "%d", config_data.TCP_port);
            packet_to_send.type = 0x04; /* 0x04 = INFO_ACK */
            strcpy(packet_to_send.mac, config_data.MAC); strcpy(packet_to_send.random, random);
            strcpy(packet_to_send.data, data_to_send);
            strcpy(controllers_data[new_pos].State, "SUBSCRIBED");
        }
        else {
            packet_to_send.type = 0x02; /* 0x02 = SUBS_REJ */
            strcpy(packet_to_send.mac, config_data.MAC); strcpy(packet_to_send.random, random);
            if (rcv_packet.type == 0x03) strcpy(packet_to_send.data, "Dades incorrectes");
            else strcpy(packet_to_send.data, "Paquet incorrecte");
            strcpy(controllers_data[pos].State, "DISCONNECTED");
            strcpy(controllers_data[pos].IP, ""); strcpy(controllers_data[pos].Random, "");
            strcpy(controllers_data[pos].Situation, ""); strcpy(controllers_data[pos].Elements, "");
        }
        sendto(sock_udp, &packet_to_send, sizeof(Packet), 0, (struct sockaddr *) &client_addr, sizeof(client_addr));

        if (debug) {
            print_msg_time("DEBUG => Enviat: "); printf("bytes=%i, comanda=%s, mac=%s, rndm=%s, dades=%s\n",
            buffer_size, packet_dictionary(packet_to_send.type), packet_to_send.mac, packet_to_send.random, packet_to_send.data);
        }
        print_msg_time("MSG.  => Controlador: "); printf("%s, passa a l'estat: %s\n", controllers_data[pos].Name, controllers_data[pos].State);

        close(new_udp_socket);
        if (debug) { print_msg_time("DEBUG => Finalitzat procés que atenia el paquet UDP\n"); }

        if (strcmp(packet_dictionary(rcv_packet.type), "SUBS_INFO") == 0 && new_pos >= 0) count_hello(new_pos);

    } else {
        strcpy(controllers_data[pos].State, "DISCONNECTED");
        strcpy(controllers_data[pos].IP, ""); strcpy(controllers_data[pos].Random, "");
        strcpy(controllers_data[pos].Situation, ""); strcpy(controllers_data[pos].Elements, "");
        print_msg_time("MSG.  => Controlador: "); printf("%s, passa a l'estat: %s\n", controllers_data[pos].Name, controllers_data[pos].State);
        close(new_udp_socket);
        if (debug) { print_msg_time("DEBUG => Finalitzat procés que atenia el paquet UDP\n"); }
    }
}

void count_hello(int pos) {
    int strike = 0;
    bool first_hello_rcv = false;
    int first_hello_strike = 0;
    int timeout = v;

    while (strike < x && first_hello_strike < r) {
        sleep(timeout);
        if (controllers_data[pos].hello == true) {
            strike = 0;
            controllers_data[pos].hello = false;
            first_hello_rcv = true;
        } else {
            strike++;
            if (!first_hello_rcv) first_hello_strike++;
        }

    }

    if (strike == x) {
        print_msg_time("MSG.  => Controlador "); printf("%s [%s] no ha rebut %i HELLO consecutius\n",
        controllers_data[pos].Name, controllers_data[pos].MAC, x);
    } else if (first_hello_strike == r) {
        print_msg_time("MSG.  => Controlador "); printf("%s [%s] no ha rebut el primer HELLO en %i segons\n",
        controllers_data[pos].Name, controllers_data[pos].MAC, v*r);
    }
    strcpy(controllers_data[pos].State, "DISCONNECTED");
    strcpy(controllers_data[pos].IP, ""); strcpy(controllers_data[pos].Random, "");
    strcpy(controllers_data[pos].Situation, ""); strcpy(controllers_data[pos].Elements, "");
    print_msg_time("MSG.  => Controlador: "); printf("%s, passa a l'estat: %s\n", controllers_data[pos].Name, controllers_data[pos].State);
}

void communication(Args arg, int pos) {
    Packet rcv_packet = arg.packet;
    Packet packet_to_send;
    struct sockaddr_in client_addr = arg.client_addr;
    controllers_data[pos].hello = true;

    if (debug) {
        print_msg_time("DEBUG => Rebut paquet: "); printf("%s del controlador: %s [%s]\n",
        packet_dictionary(rcv_packet.type), controllers_data[pos].Name, controllers_data[pos].MAC);
    }

    /* Build packet to send */
    packet_to_send.type = 0x10; /* 0x10 = HELLO */
    strcpy(packet_to_send.mac, config_data.MAC);
    strcpy(packet_to_send.random, controllers_data[pos].Random);
    strcpy(packet_to_send.data, rcv_packet.data);

    /* Send the packet */
    sendto(sock_udp, &packet_to_send, sizeof(Packet), 0, (struct sockaddr *) &client_addr,
           sizeof(client_addr));

    if (debug) {
        print_msg_time("DEBUG => Enviat: ");
        printf("bytes=%i, comanda=%s, mac=%s, rndm=%s, dades=%s\n",
               buffer_size, packet_dictionary(packet_to_send.type), packet_to_send.mac,
               packet_to_send.random, packet_to_send.data);
    }
    if (strcmp(controllers_data[pos].State, "SUBSCRIBED") == 0) {
        strcpy(controllers_data[pos].State, "SEND_HELLO");
        print_msg_time("MSG.  => Controlador: "); printf("%s, passa a l'estat: %s\n", controllers_data[pos].Name, controllers_data[pos].State);
    }
    if (debug) { print_msg_time("DEBUG => Finalitzat procés que atenia el paquet UDP\n"); }
}

void send_hello_rej(Packet rcv_packet, int pos, struct sockaddr_in client_addr) {
    Packet packet_to_send;

    /* Build packet to send */
    packet_to_send.type = 0x11; /* 0x11 = HELLO_REJ */
    strcpy(packet_to_send.mac, config_data.MAC); strcpy(packet_to_send.random, "00000000");
    strcpy(packet_to_send.data, "Dades incorrectes");
    strcpy(controllers_data[pos].State, "DISCONNECTED");
    strcpy(controllers_data[pos].IP, ""); strcpy(controllers_data[pos].Random, "");
    strcpy(controllers_data[pos].Situation, ""); strcpy(controllers_data[pos].Elements, "");
    print_msg_time("MSG.  => Controlador: "); printf("%s, passa a l'estat: %s\n", controllers_data[pos].Name, controllers_data[pos].State);

    /* Send the packet */
    sendto(sock_udp, &packet_to_send, sizeof(Packet), 0, (struct sockaddr *) &client_addr,
    sizeof(client_addr));

    if (debug) {
        print_msg_time("DEBUG => Enviat: ");
        printf("bytes=%i, comanda=%s, mac=%s, rndm=%s, dades=%s\n",
               buffer_size, packet_dictionary(packet_to_send.type), packet_to_send.mac,
               packet_to_send.random, packet_to_send.data);
    }
}

int main(int argc, char *argv[]) {
    read_entry_parameters(argc, argv);
    read_controllers();
    read_config();
    pthread_create(&console, NULL, system_input, NULL);
    udp_socket(); if (debug) { print_msg_time("DEBUG => Socket UDP actiu\n"); }
    tcp_socket(); if (debug) { print_msg_time("DEBUG => Socket TCP actiu\n"); }
    if (debug) {
        print_msg_time("DEBUG => Creat fill per gestionar la BBDD de controladors\n");
        print_msg_time("INFO  => Establert temporitzador per la gestió de la BBDD\n");
    }
    read_packet();
    return 0;
}

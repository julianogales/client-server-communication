#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
} ControllersData;
ControllersData controllers_data;

/* ----------------------------------------------------------------------------------------------------------------- */



/* Read file 'server.cfg' and save the information (Name, MAC, UDP-port, TCP-port) inside config_data.*/
void read_config() {
    FILE *file = fopen("server.cfg", "r");
    char line[25];

    while (fgets(line, sizeof(line), file)) {
        char *key = strtok(line, " =");
        char *val = strtok(NULL, " =\n");

        if (strcmp(key, "Name") == 0) { strcpy(config_data.Name, val);}
        else if (strcmp(key, "MAC") == 0) {strcpy(config_data.MAC, val);}
        else if (strcmp(key, "UDP-port") == 0) {config_data.UDP_port = atoi(val);}
        else if (strcmp(key, "TCP-port") == 0) {config_data.TCP_port = atoi(val);}
    }

    fclose(file);
}

/* Read file 'server.cfg' and save the information (Name, MAC, UDP-port, TCP-port) inside config_data.*/
void read_controllers() {
    FILE *file = fopen("controllers.dat", "r");
    char line[25];

    while (fgets(line, sizeof(line), file)) {
        char *key = strtok(line, ",");
        char *val = strtok(NULL, "\n");

        if (strcmp(key, "Name") == 0) { strcpy(config_data.Name, val);}
        else if (strcmp(key, "MAC") == 0) {strcpy(config_data.MAC, val);}
        else if (strcmp(key, "State") == 0) {config_data.UDP_port = atoi(val);}
    }

    fclose(file);
}

int main() {
    read_config();
    return 0;
}

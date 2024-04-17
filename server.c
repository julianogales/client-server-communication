// Read file 'client.cfg' and save the information (Name, Situation, Elements, MAC, Local, Server, Srv-UDP) inside
// config_data
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
def read_config():
    file = open('client.cfg', 'r')
    try:
        for line in file:
            key, value = line.strip().split(' = ')

            config_data[key] = value
    finally:
        file.close()
*/


// Read file 'server.cfg' and save the information (Name, MAC, UDP-port, TCP-port) inside config_data.
void read_config() {
    FILE *file = fopen("server.cfg", "r");
    char line[25];
    char config_data;

    while (fgets(line, sizeof(line), file)) {
        char *key = strtok(line, " =");
        char *val = strtok(NULL, " =\n");
        printf("%s : %s\n", key, val);
    }

    fclose(file);
}

int main() {
    read_config();
}

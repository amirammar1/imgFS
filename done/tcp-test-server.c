#include <stddef.h> // size_t
#include <stdint.h> // uint16_t
#include <stdio.h>
#include <unistd.h>



#include "imgfs.h"
#include "error.h"
#include "socket_layer.h"
#include "util.h"   // for _unused
#include <string.h>

const int MAX_FILE_SIZE = 1024;
const char DELIMITER_SIZE[] = "|";
const char DELIMITER_FILE[] = "<EOF>";

int main(int argc, char** argv)
{
    argc--;
    argv++;
    if(argc<1)return ERR_NOT_ENOUGH_ARGUMENTS;
    if(argc>1)return ERR_INVALID_COMMAND;
    uint16_t port = atouint16(argv[0]);
    printf("\nServer started on port %d\n",port);
    int id_socket = tcp_server_init(port);
    if (id_socket<0) return id_socket;
    while (1) {
        printf("Waiting for a size...\n");
        int id_client = tcp_accept(id_socket);
        if(id_client<0) {
            close(id_socket);
            return id_client;
        }
        uint16_t size;

        char str_size_delimit[6] = {'\0'};
        if(tcp_read(id_client,str_size_delimit,sizeof(str_size_delimit))<=0) {
            close(id_client);
            perror("error when receiving size file");
            return ERR_IO;
        }
        char* delim_size_file = strstr(str_size_delimit,DELIMITER_SIZE);
        if(delim_size_file == NULL) {
            close(id_client);
            return ERR_IO;
        }
        int len_size = (int)(delim_size_file - str_size_delimit);

        char str_size[len_size+1];
        strncpy(str_size,str_size_delimit,len_size);
        str_size[len_size] = '\0';
        int content_read = (int)(strlen(str_size_delimit) - len_size - strlen(DELIMITER_SIZE));

        size = atouint16(str_size);
        printf("Received a size: %d --> ",size);
        char acknowledge[15];
        if(size< MAX_FILE_SIZE) {
            strcpy(acknowledge, "Small file");
            printf("accepted\n");

        } else {
            strcpy(acknowledge, "Big file");
            printf("denied\n");
            continue;

        }

        if(tcp_send(id_client, acknowledge, sizeof(acknowledge)) <= 0) {
            perror("error when sending acknowledge 2.0");
            close(id_socket);
            close(id_client);
            continue;
        }
        printf("About to receive file of %d bytes\n",size);
        int file_size = size - content_read + strlen(DELIMITER_FILE);
        char* file = malloc(file_size+1);
        char acknowledge2[] = "OK";
        int read_data = 0;
        while (read_data < (file_size)) {
            int read_d = tcp_read(id_client,file,file_size);
            if(read_d<=0) {
                close(id_socket);
                close(id_client);
                strcpy(acknowledge2, "NO");
                perror("error when receiving file");
                break;
            } else {
                read_data += read_d;
                file+=read_d;
            }

        }

        if(tcp_send(id_client, acknowledge2, sizeof(acknowledge2)) <= 0) {
            close(id_socket);
            close(id_client);
            perror("error when sending acknowledgement 2.0");
            free(file);
            continue;
        }
        if(strcmp(acknowledge2, "NO") == 0)continue;
        file-=read_data;
        file[file_size]='\0';
        char* delim_pos = strstr(file,DELIMITER_FILE);
        *delim_pos = '\0';
        printf("Received a file:\n");
        if(content_read !=0)printf("%s", str_size_delimit+(len_size+1));
        printf("%s\n",file);
        free(file);

    }


}

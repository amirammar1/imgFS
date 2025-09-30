/*
 * @file http_net.c
 * @brief HTTP server layer for CS-202 project
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h> //multithreading

#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "error.h"

static int passive_socket = -1;
static EventCallback cb;

#define MK_OUR_ERR(X) \
static int our_ ## X = X
#define MAX_BODY_SIZE_STR 20

MK_OUR_ERR(ERR_NONE);
MK_OUR_ERR(ERR_INVALID_ARGUMENT);
MK_OUR_ERR(ERR_OUT_OF_MEMORY);
MK_OUR_ERR(ERR_IO);

/**
 * @brief Personal added method to read a message from a socket.
 *
 * @param socket_ID The socket ID.
 * @param buffer The buffer to read into.
 * @param header_read The amount of header data read.
 * @param MAX_READ The maximum amount of data to read.
 * @return A pointer to the error code.
 */
int* read_message(int socket_ID, char* buffer, int* header_read, const int MAX_READ)
{
    char *buffer_to_read = buffer + *header_read;
    int check_read = *header_read;
    int data_read = 0;
    while (data_read < MAX_READ) {
        int max_left_data = MAX_READ - data_read;
        if (check_read == 0 && strstr(buffer, HTTP_HDR_END_DELIM) != NULL) break;
        int data_tcp_read = (int) tcp_read(socket_ID, buffer_to_read, (size_t) max_left_data);
        if (data_tcp_read <= 0) {
            return &our_ERR_IO;
        }
        data_read += data_tcp_read;
        buffer_to_read += data_tcp_read;
    }
    *header_read += data_read;
    return &our_ERR_NONE;
}

/*******************************************************************
 * Handle connection
 */
static void *handle_connection(void *arg)
{
    if (arg == NULL) return &our_ERR_INVALID_ARGUMENT;

    // Block SIGINT and SIGTERM signals for the thread
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    int header_read = 0;
    int socket_ID = *((int *) arg);
    while (1) {
        char *buffer = malloc(MAX_HEADER_SIZE);
        if (buffer == NULL) {
            perror("Allocation problem when handling connection");
            free(arg);
            return &our_ERR_OUT_OF_MEMORY;
        }
        int *err_read = read_message(socket_ID, buffer, &header_read, MAX_HEADER_SIZE);
        if (*err_read != our_ERR_NONE) {
            perror("Reading the message failed");
            close(socket_ID);
            free(buffer);
            free(arg);
            return &our_ERR_NONE;
        }

        struct http_message message_out;
        int content_len;
        int return_parse = http_parse_message(buffer, (size_t) header_read, &message_out, &content_len);
        if (return_parse < 0) {
            perror("Parsing header message failed");
            close(socket_ID);
            free(buffer);
            free(arg);
            return &return_parse;
        }
        if (return_parse == 0) {
            char* delim = strstr(buffer, HTTP_HDR_END_DELIM);
            if(delim==NULL) {
                perror("Header does not contain delimiter");
                close(socket_ID);
                free(buffer);
                free(arg);
                return &our_ERR_IO;
            }
            int body_already_read = header_read - (delim - buffer) - strlen(HTTP_HDR_END_DELIM);

            size_t new_size = header_read + content_len - body_already_read ;
            char *temp = realloc(buffer, new_size);
            if (temp == NULL) {
                perror("Problem when reallocating the memory for the body of message");
                close(socket_ID);
                free(buffer);
                free(arg);
                return &our_ERR_IO;
            }
            buffer = temp;

            int* err = read_message(socket_ID, buffer, &header_read, content_len - body_already_read);
            if (*err != our_ERR_NONE) {
                perror("Reading the body failed");
                close(socket_ID);
                free(buffer);
                free(arg);
                return err;
            }
            return_parse = http_parse_message(buffer, (size_t) header_read, &message_out, &content_len);
            if (return_parse < 0) {
                perror("Parsing the whole message failed");
                close(socket_ID);
                free(arg);
                free(buffer);
                return &return_parse;
            }
            if (return_parse == 0) {
                perror("Message incomplete");
                close(socket_ID);
                free(buffer);
                free(arg);
                return &our_ERR_NONE;
            }
        }
        if (return_parse > 0) {
            int output_reply = cb(&message_out, socket_ID);
            if (output_reply != ERR_NONE) {
                close(socket_ID);
                free(buffer);
                free(arg);
                if (output_reply == -1) {
                    return &our_ERR_NONE;
                } else return &output_reply;
            }

            content_len = 0;
            header_read = 0;
            free(buffer);
        }

    }
}

/*******************************************************************
 * Init connection
 */
int http_init(uint16_t port, EventCallback callback)
{
    passive_socket = tcp_server_init(port);
    cb = callback;
    return passive_socket;
}

/*******************************************************************
 * Close connection
 */
void http_close(void)
{
    if (passive_socket > 0) {
        if (close(passive_socket) == -1)
            perror("error in http_close()");
        else
            passive_socket = -1;
    }
}

/*******************************************************************
 * Receive content
 */
int http_receive(void)
{
    // Initialize thread attributes for multithreading
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int *active_socket = malloc(sizeof(int));
    if (active_socket == NULL) {
        perror("malloc error when receiving content");
        return ERR_OUT_OF_MEMORY;
    }

    *active_socket = tcp_accept(passive_socket);
    if (*active_socket < 0) {
        perror("tcp_accept");
        free(active_socket);
        return ERR_IO;
    }

    pthread_t thread;
    if (pthread_create(&thread, &attr, handle_connection, active_socket) != 0) {
        perror("pthread_create");
        close(*active_socket);
        free(active_socket);
        return ERR_THREADING;
    }

    if (pthread_attr_destroy(&attr) != 0) return ERR_THREADING;

    return ERR_NONE;
}

/*******************************************************************
 * Serve a file content over HTTP
 */
int http_serve_file(int connection, const char* filename)
{
    M_REQUIRE_NON_NULL(filename);

    // open file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to open file \"%s\"\n", filename);
        return http_reply(connection, "404 Not Found", "", "", 0);
    }

    // get its size
    fseek(file, 0, SEEK_END);
    const long pos = ftell(file);
    if (pos < 0) {
        fprintf(stderr, "http_serve_file(): Failed to tell file size of \"%s\"\n",
                filename);
        fclose(file);
        return ERR_IO;
    }
    rewind(file);
    const size_t file_size = (size_t) pos;

    // read file content
    char* const buffer = calloc(file_size + 1, 1);
    if (buffer == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to allocate memory to serve \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    const size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "http_serve_file(): Failed to read \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    // send the file
    const int  ret = http_reply(connection, HTTP_OK,
                                "Content-Type: text/html; charset=utf-8" HTTP_LINE_DELIM,
                                buffer, file_size);

    // garbage collecting
    fclose(file);
    free(buffer);
    return ret;
}


/*******************************************************************
 * Create and send HTTP reply
 */
int http_reply(int connection, const char *status, const char *headers, const char *body, size_t body_len)
{
    char body_len_str[MAX_HEADER_SIZE];
    sprintf(body_len_str, "%zu",body_len);
    size_t reply_size = strlen(HTTP_PROTOCOL_ID) + strlen(status) + strlen(headers)+
                        strlen(HTTP_LINE_DELIM) + strlen("Content-length: ")
                        + strlen(body_len_str) + strlen(HTTP_HDR_END_DELIM);

    char *reply = malloc(reply_size+1);
    if(reply==NULL){
        perror("Error when allocating memory reply");
        return ERR_OUT_OF_MEMORY;
    }
    sprintf(reply, "%s%s%s%sContent-Length: %zu%s", HTTP_PROTOCOL_ID, status, HTTP_LINE_DELIM, headers, body_len,
            HTTP_HDR_END_DELIM);

    ssize_t sending = tcp_send(connection, reply, reply_size);
    if (body_len != 0)tcp_send(connection,body,body_len);
    free(reply);
    reply = NULL;
    if (sending < 0)return ERR_IO;
    if(strcmp(status,HTTP_OK) !=0)return -1;
    return ERR_NONE;
}

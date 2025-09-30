#include <stddef.h> // size_t
#include <stdint.h> // uint16_t
#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in structure
#include <stdio.h> // For standard I/O functions
#include <unistd.h> // For close function

#include "imgfs.h" // Custom header, probably for file system operations
#include "error.h" // Custom header for error codes
#include "socket_layer.h" // Custom header for socket operations
#include "util.h" // Custom header, for atouint16 function and _unused macro
#include <string.h> // For string functions
#include <arpa/inet.h> // For inet_addr function

#define SERVER_IP "127.0.0.1" // Local address of the server

const int MAX_FILE_SIZE = 2048; // Maximum allowed file size
const char DELIMITER_SIZE[] = "|"; // Delimiter for size information
const char DELIMITER_FILE[] = "<EOF>"; // Delimiter for end of file

int main(int argc, char** argv)
{
    argc--; // Decrease argument count to skip the program name
    argv++; // Move to the first actual argument

    // Check if the number of arguments is less than 2 or more than 2
    if(argc < 2) return ERR_NOT_ENOUGH_ARGUMENTS;
    if(argc > 2) return ERR_INVALID_COMMAND;

    // Open the file in binary read mode
    FILE* file = fopen(argv[1], "rb");
    if(file == NULL) {
        perror("File doesn't exist"); // Error message if file doesn't exist
        return ERR_IO;
    }

    // Move the file pointer to the end of the file
    fseek(file, 0, SEEK_END);
    // Get the size of the file
    uint16_t size = (uint16_t) ftell(file);
    // Check if the file size exceeds the maximum allowed size
    if(size >= MAX_FILE_SIZE) {
        perror("Very big file"); // Error message if file is too large
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    // Convert the first argument to a port number
    uint16_t port = atouint16(argv[0]);
    printf("\nTalking to %d\n", port); // Print the port number

    // Create a socket
    int socket_ID = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_ID == -1) {
        perror("Impossible to create socket"); // Error message if socket creation fails
        return ERR_IO;
    }

    // Set up the socket address structure
    struct sockaddr_in socket_address;
    socket_address.sin_port = htons(port); // Set port number
    socket_address.sin_family = AF_INET; // Set address family

    socket_address.sin_addr.s_addr = inet_addr(SERVER_IP); // Set server IP address

    // Attempt to connect to the server
    if (connect(socket_ID, (struct sockaddr *)&socket_address, sizeof(socket_address)) < 0) {
        perror("Error when connecting to the server"); // Error message if connection fails
        exit(EXIT_FAILURE);
    }

    printf("Sending size %d\n", size); // Print the size being sent
    char size_str[6];
    sprintf(size_str, "%d|", size); // Format the size string with delimiter
    // Send the size string to the server
    if(tcp_send(socket_ID, size_str, sizeof(size_str)) <= 0) {
        perror("Error when sending size"); // Error message if sending fails
        close(socket_ID);
        return ERR_IO;
    }

    char buffer[15];
    // Read the server's response
    if(tcp_read(socket_ID, buffer, sizeof(buffer)) <= 0) {
        close(socket_ID);
        perror("Error when receiving acknowledgement"); // Error message if reading fails
        return ERR_IO;
    }

    printf("Server responded: \"%s\"\n", buffer); // Print the server's response
    // Check if the response is not "Small file"
    if(strcmp("Small file", buffer) != 0) {
        perror("Size not accepted by server"); // Error message if size not accepted
        close(socket_ID);
        return ERR_IO;
    }

    printf("Sending %s :\n", argv[1]); // Print the file being sent
    // Allocate memory for the file content plus delimiter
    char* file_str = malloc(size + strlen(DELIMITER_FILE) + 1);
    fseek(file, 0, SEEK_SET); // Move the file pointer to the start
    // Read the file content into the buffer
    if(fread(file_str, size, 1, file) != 1) {
        free(file_str); // Free allocated memory if read fails
        return ERR_IO;
    }
    file_str += size; // Move pointer to the end of the content
    strcpy(file_str, DELIMITER_FILE); // Append the delimiter
    file_str -= size; // Move pointer back to the start of the content

    // Send the file content plus delimiter to the server
    if(tcp_send(socket_ID, file_str, size + strlen(DELIMITER_FILE)) <= 0) {
        free(file_str); // Free allocated memory if send fails
        perror("Error when sending file"); // Error message if sending fails
        close(socket_ID);
        return ERR_IO;
    }
    free(file_str); // Free the allocated memory

    char acknowledge[3];
    // Read the server's final acknowledgement
    if(tcp_read(socket_ID, acknowledge, sizeof(acknowledge)) <= 0) {
        close(socket_ID);
        perror("Error when receiving acknowledgement 2.0"); // Error message if reading fails
        return ERR_IO;
    }
    close(socket_ID); // Close the socket
    // Check if the final acknowledgement is not "OK"
    if(strcmp(acknowledge, "OK") != 0) {
        perror("Acknowledgement 2.0 not valid"); // Error message if acknowledgement is invalid
        return ERR_IO;
    }
    printf("Accepted\nDone\n"); // Print success message
    return ERR_NONE; // Return success code
}

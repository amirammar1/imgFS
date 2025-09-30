#include <stddef.h> // For size_t
#include <stdint.h> // For uint16_t
#include <sys/types.h> // For ssize_t
#include <sys/socket.h> // For socket functions and structures
#include <netinet/in.h> // For sockaddr_in structure and constants
#include <stdio.h> // For perror
#include <unistd.h> // For close function

#include "imgfs.h" // Custom header file
#include "error.h" // Custom header file for error handling

/*******************************************************************
 * Initialize TCP connection
 */
int tcp_server_init(uint16_t port)
{

    // Create a TCP socket
    int socket_ID = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_ID == -1) {
        perror("Impossible to create socket");
        return ERR_IO;
    }

    // Define the socket address structure
    struct sockaddr_in socket_address;
    socket_address.sin_port = htons(port); // Set port number
    socket_address.sin_family = AF_INET; // Set address family
    socket_address.sin_addr.s_addr = htonl(INADDR_ANY); // Set IP address to any available interface

    // Added block to avoid address block due to already used port
    int opt = 1;
    if (setsockopt(socket_ID, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Problem when setting reused adress");
        close(socket_ID);
        return ERR_IO;
    }

    // Bind the socket to the address and port
    if(bind(socket_ID, (struct sockaddr*) &socket_address, sizeof(socket_address)) == -1) {
        close(socket_ID); // Close socket
        perror("Problem when binding occurred");
        return ERR_IO;
    }

    // Listen for incoming connections
    if(listen(socket_ID, SOMAXCONN) == -1) {
        close(socket_ID);
        perror("Error in listening");
        return ERR_IO;
    }

    return socket_ID;
}

/*******************************************************************
 * Accept a new connection on the passive socket
 */
int tcp_accept(int passive_socket)
{
    int return_accept = accept(passive_socket, NULL, NULL);
    return return_accept;
}

/*******************************************************************
 * Read data from the active socket into the buffer
 */
ssize_t tcp_read(int active_socket, char* buf, size_t buflen)
{
    M_REQUIRE_NON_NULL(buf); // Ensure buffer is not NULL
    if(active_socket < 0 ) {
        return ERR_INVALID_ARGUMENT; // Return invalid argument error code
    }
    return recv(active_socket, buf, buflen, 0); // Receive data from the socket
}

/*******************************************************************
 * Send data to the active socket
 */
ssize_t tcp_send(int active_socket, const char* response, size_t response_len)
{
    M_REQUIRE_NON_NULL(response); // Ensure response is not NULL
    if(active_socket < 0 ) {
        return ERR_INVALID_ARGUMENT; // Return invalid argument error code
    }
    return send(active_socket, response, response_len, 0); // Send data to the socket
}

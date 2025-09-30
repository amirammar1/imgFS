/*
 * @file imgfs_server_services.c
 * @brief ImgFS server part, bridge between HTTP server layer and ImgFS library
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint16_t
#include <vips/vips.h>
#include <pthread.h> //multithreading

#include "error.h"
#include "util.h" // atouint16
#include "imgfs.h"
#include "http_net.h"
#include "imgfs_server_service.h"


// Main in-memory structure for imgFS
static struct imgfs_file fs_file;
static uint16_t server_port;

static pthread_mutex_t imgfs_mutex;

#define URI_ROOT "/imgfs"

/********************************************************************//**
 * Startup function. Create imgFS file and load in-memory structure.
 * Pass the imgFS file name as argv[1] and optionnaly port number as argv[2]
 ********************************************************************** */
int server_startup(int argc, char **argv)
{
    // Check if the required number of arguments is provided
    if (argc < 1) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    if(argc > 2){
        return ERR_INVALID_COMMAND;
    }

    // Initialize the VIPS image processing library
    int ret = VIPS_INIT(argv[0]);
    if (ret != ERR_NONE) {
        return ERR_IMGLIB;
    }

    // Initialize the mutex for multithreading
    if (pthread_mutex_init(&imgfs_mutex, NULL) != 0) {
        fprintf(stderr, "Failed to initialize mutex\n");
        vips_shutdown();
        return ERR_THREADING;
    }

    // Open the file system file
    int error_open = do_open(argv[0], "rb+", &fs_file);
    if (error_open < 0) {
        vips_shutdown(); // Shut down the VIPS library
        pthread_mutex_destroy(&imgfs_mutex); // Destroy the mutex
        return ERR_INVALID_FILENAME;
    }

    // Print the file system header
    print_header(&fs_file.header);

    // Determine the server port
    if (argc > 1) {
        server_port = atouint16(argv[1]);
        if (server_port == 0) server_port = DEFAULT_LISTENING_PORT;
    } else {
        server_port = DEFAULT_LISTENING_PORT;
    }

    // Initialize the HTTP server and check for errors
    int error_init = http_init(server_port, handle_http_message);
    if (error_init < 0) {
        do_close(&fs_file);
        vips_shutdown(); // Shut down the VIPS library
        pthread_mutex_destroy(&imgfs_mutex); // Destroy the mutex
        return error_init;
    }

    // Print the server start message
    printf("ImgFS server started on http://localhost:%d\n", server_port);
    return ERR_NONE;
}

/********************************************************************//**
 * Shutdown function. Free the structures and close the file.
 ********************************************************************** */
void server_shutdown(void)
{
    fprintf(stderr, "Shutting down...\n");
    vips_shutdown(); // Shut down the VIPS library
    http_close(); // Close the HTTP server
    do_close(&fs_file); // Close the file system file
    pthread_mutex_destroy(&imgfs_mutex); // Destroy the mutex
}


/**********************************************************************
 * Sends error message.
 ********************************************************************** */
static int reply_error_msg(int connection, int error)
{
#define ERR_MSG_SIZE 256
    char err_msg[ERR_MSG_SIZE]; // enough for any reasonable err_msg
    if (snprintf(err_msg, ERR_MSG_SIZE, "Error: %s\n", ERR_MSG(error)) < 0) {
        fprintf(stderr, "reply_error_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "500 Internal Server Error", "",
                      err_msg, strlen(err_msg));
}

/**********************************************************************
 * Sends 302 OK message.
 ********************************************************************** */
static int reply_302_msg(int connection)
{
    char location[ERR_MSG_SIZE];
    if (snprintf(location, ERR_MSG_SIZE, "Location: http://localhost:%d/" BASE_FILE HTTP_LINE_DELIM,
                 server_port) < 0) {
        fprintf(stderr, "reply_302_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "302 Found", location, "", 0);
}

/**********************************************************************
 * Simple handling of http message. TO BE UPDATED WEEK 13
 ********************************************************************** */
int handle_http_message(struct http_message* msg, int connection)
{
    M_REQUIRE_NON_NULL(msg);
    debug_printf("handle_http_message() on connection %d. URI: %.*s\n",
                 connection,
                 (int) msg->uri.len, msg->uri.val);
    if (http_match_verb(&msg->uri, "/") || http_match_uri(msg, "/index.html")) {
        return http_serve_file(connection, BASE_FILE);
    }

    if (http_match_uri(msg, URI_ROOT "/list") ) {
        return handle_list_call(connection);
    }  else if (http_match_uri(msg, URI_ROOT "/read")  ) {
        return handle_read_call( connection, msg);
    }  else if (http_match_uri(msg, URI_ROOT "/delete")) {
        return handle_delete_call( connection, msg);
    }  else if (http_match_uri(msg, URI_ROOT "/insert")
                && http_match_verb(&msg->method, "POST")) {
        return handle_insert_call( connection, msg);
    } else
        return reply_error_msg(connection, ERR_INVALID_COMMAND);
}
/**********************************************************************
 * Handle the list command.
 ********************************************************************** */
static int handle_list_call(int connection)
{
    char *json_response = NULL;
    pthread_mutex_lock(&imgfs_mutex); // Acquire mutex lock for thread safety
    int result = do_list(&fs_file, JSON, &json_response); // Get the list in JSON format
    pthread_mutex_unlock(&imgfs_mutex); // Release mutex lock

    if (result != ERR_NONE) {
        if(json_response!=NULL)free(json_response);
        return reply_error_msg(connection, result); // Reply with error message if any
    }

    // Create HTTP headers for the response
    char headers[MAX_HEADER_SIZE];
    if (snprintf(headers, sizeof(headers),
                 "Content-Type: application/json" HTTP_LINE_DELIM) < 0) {
        free(json_response);
        return reply_error_msg(connection, ERR_RUNTIME); // Reply with runtime error message
    }

    // Send HTTP response with JSON data
    result = http_reply(connection, HTTP_OK, headers, json_response, strlen(json_response));
    free(json_response); // Free the allocated memory for JSON response

    return result;
}


/**********************************************************************
 * Handle the read command.
 ********************************************************************** */
static int handle_read_call(int connection, struct http_message *msg)
{
    char resolution_str[10]; // Buffer to store the resolution string
    char image_id[MAX_IMG_ID]; // Buffer to store the image ID

    // Get the resolution from the URI
    int result = http_get_var(&msg->uri, "res", resolution_str, sizeof(resolution_str));
    if(result == ERR_RUNTIME)return reply_error_msg(connection, ERR_RESOLUTIONS); // Reply with error if resolution is too long;
    else if (result <= 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS); // Reply with error if resolution is missing
    }

    // Get the image ID from the URI
    result = http_get_var(&msg->uri, "img_id", image_id, sizeof(image_id));
    if(result == ERR_RUNTIME)return reply_error_msg(connection, ERR_INVALID_IMGID); // Reply with error if name is too long;
    else if (result <= 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS); // Reply with error if image ID is missing
    }

    int resolution = resolution_atoi(resolution_str); // Convert resolution string to integer
    if (resolution < 0) {
        return reply_error_msg(connection, ERR_RESOLUTIONS); // Reply with error if resolution is invalid
    }

    pthread_mutex_lock(&imgfs_mutex); // Acquire mutex lock for thread safety
    char *image_data = NULL;
    uint32_t image_size = 0;
    // Read the image data with the specified resolution
    result = do_read(image_id, resolution, &image_data, &image_size, &fs_file);
    pthread_mutex_unlock(&imgfs_mutex); // Release mutex lock

    if (result != ERR_NONE) {
        if(image_data != NULL)free(image_data);
        return reply_error_msg(connection, result); // Reply with error if reading fails
    }

    // Create HTTP headers for the response
    char headers[MAX_HEADER_SIZE];
    if (snprintf(headers, sizeof(headers),
                 "Content-Type: image/jpeg" HTTP_LINE_DELIM) < 0) {
        free(image_data); // Free image data if header creation fails
        return reply_error_msg(connection, ERR_RUNTIME); // Reply with runtime error message
    }

    // Send HTTP response with image data
    result = http_reply(connection, HTTP_OK, headers, image_data, image_size);

    free(image_data); // Free the allocated memory for image data

    return result;
}

/**********************************************************************
 * Handle the delete command.
 ********************************************************************** */
static int handle_delete_call(int connection, struct http_message *msg)
{
    char image_id[MAX_IMG_ID]; // Buffer to store the image ID
    // Get the image ID from the URI
    int result = http_get_var(&msg->uri, "img_id", image_id, sizeof(image_id));
    if(result == ERR_RUNTIME)return reply_error_msg(connection, ERR_INVALID_IMGID); // Reply with error if name is too long;
    else if (result <= 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS); // Reply with error if image ID is missing
    }

    pthread_mutex_lock(&imgfs_mutex); // Acquire mutex lock for thread safety
    result = do_delete(image_id, &fs_file); // Delete the image
    pthread_mutex_unlock(&imgfs_mutex); // Release mutex lock

    if (result != ERR_NONE) {
        return reply_error_msg(connection, result); // Reply with error if deletion fails
    }

    return reply_302_msg(connection); // Reply with 302 status (redirect)
}

/**********************************************************************
 * Handle the insert command.
 ********************************************************************** */
static int handle_insert_call(int connection, struct http_message *msg)
{
    char image_name[MAX_IMG_ID]; // Buffer to store the name
    // Get the name from the URI
    int result = http_get_var(&msg->uri, "name", image_name, MAX_IMG_ID);
    if(result == ERR_RUNTIME)return reply_error_msg(connection, ERR_INVALID_FILENAME); // Reply with error if name is too long;
    else if (result <= 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS); // Reply with error if name is missing
    }

    // Allocate memory for the data to be inserted
    char *image_data = malloc(msg->body.len);
    if (image_data == NULL) {
        return reply_error_msg(connection, ERR_OUT_OF_MEMORY); // Reply with error if memory allocation fails
    }
    memcpy(image_data, msg->body.val, msg->body.len); // Copy the data from the message body

    pthread_mutex_lock(&imgfs_mutex); // Acquire mutex lock for thread safety
    // Insert the data with the specified name
    result = do_insert(image_data, msg->body.len, image_name, &fs_file);
    pthread_mutex_unlock(&imgfs_mutex); // Release mutex lock

    free(image_data); // Free the allocated memory for data

    if (result != ERR_NONE) {
        return reply_error_msg(connection, result); // Reply with error if insertion fails
    }

    return reply_302_msg(connection); // Reply with 302 status (redirect)
}


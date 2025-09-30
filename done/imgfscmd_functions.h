/**
 * @file imgfscmd_functions.h
 * @brief interface function for imgFS command line interpreter.
 *
 * @author Ludovic Mermod
 */

#pragma once

/**********************************************************************
 * Displays some explanations.
 ********************************************************************** */
int help (int, char*[]);

/********************************************************************
 * Opens imgFS file and calls do_list().
 *******************************************************************/
int do_list_cmd(int argc, char* argv[]);

/********************************************************************
 * Prepares and calls do_create command.
 *******************************************************************/
int do_create_cmd(int argc, char* argv[]);

/********************************************************************
 * Deletes an image from the imgFS.
 *******************************************************************/
int do_delete_cmd(int argc, char* argv[]);

/********************************************************************
 * Inserts an image into the imgFS.
 *******************************************************************/
int do_insert_cmd(int argc, char* argv[]);

/********************************************************************
 * Reads an image from the imgFS.
 *******************************************************************/
int do_read_cmd(int argc, char* argv[]);

/********************************************************************
 * Verifies and puts the resolution.
 *******************************************************************/
int verify_and_put_resolution(uint16_t new_data, uint16_t* emplacement, uint16_t max_resolution );

/********************************************************************
 * Creates the name of the file.
 *******************************************************************/
static void create_name(const char* img_id, int resolution, char** new_name);

/********************************************************************
 * Writes the image buffer and image size in the file.
 *******************************************************************/
static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size);

/********************************************************************
 * Verifies and puts the resolution.
 *******************************************************************/
static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size);

/**********************************************************************
 * verify the resolution and put it in it's corresponding variable.
********************************************************************** */
int verify_and_put_resolution(uint16_t new_data, uint16_t* emplacement, uint16_t max_resolution );

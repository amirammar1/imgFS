/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// default values
static const uint32_t default_max_files = 128;
static const uint16_t default_thumb_res = 64;
static const uint16_t default_small_res = 256;

// max values
static const uint16_t MAX_THUMB_RES = 128;
static const uint16_t MAX_SMALL_RES = 512;
static const uint32_t MAX_MAX_FILES = (uint32_t) -1;

/**********************************************************************
 * Displays some explanations.
 ********************************************************************** */
int help(int useless _unused, char **useless_too _unused)
{
    printf("imgfscmd [COMMAND] [ARGUMENTS]\n");
    printf("  help: displays this help.\n");
    printf("  list <imgFS_filename>: list imgFS content.\n");
    printf("  create <imgFS_filename> [options]: create a new imgFS.\n");
    printf("      options are:\n");
    printf("          -max_files <MAX_FILES>: maximum number of files.\n");
    printf("                                  default value is %u\n", default_max_files);
    printf("                                  maximum value is %u\n", MAX_MAX_FILES);
    printf("          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n");
    printf("                                  default value is %ux%u\n", default_thumb_res, default_thumb_res);
    printf("                                  maximum value is %ux%u\n", MAX_THUMB_RES, MAX_THUMB_RES);
    printf("          -small_res <X_RES> <Y_RES>: resolution for small images.\n");
    printf("                                  default value is %ux%u\n", default_small_res, default_small_res);
    printf("                                  maximum value is %ux%u\n", MAX_SMALL_RES, MAX_SMALL_RES);
    printf("  read   <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n");
    printf("      read an image from the imgFS and save it to a file.\n");
    printf("      default resolution is \"original\".\n");
    printf("  insert <imgFS_filename> <imgID> <filename>: insert a new image in the imgFS.\n");
    printf("  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n");

    return ERR_NONE;
}

/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc < 1) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    if (argc > 1) {
        return ERR_INVALID_COMMAND;
    }

    struct imgfs_file img_file;

    int err_code = do_open(argv[0], "rb", &img_file);
    if (err_code != ERR_NONE) {
        return err_code;
    }
    err_code = do_list(&img_file, STDOUT, NULL);
    if (err_code != ERR_NONE) {
        return err_code;
    }

    do_close(&img_file);
    return ERR_NONE;
}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */
int do_create_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc == 0)return ERR_NOT_ENOUGH_ARGUMENTS;

    //Initialize file name and put default value for different variables
    char *filename = argv[0];
    struct imgfs_file imgfs_file;
    int counter = 0;
    char **argv_copy = argv;
    argv_copy++;
    argc--;
    imgfs_file.header.max_files = default_max_files;
    for (int i = 0; i < 2; ++i) {
        imgfs_file.header.resized_res[i] = default_thumb_res;
        imgfs_file.header.resized_res[i + 2] = default_small_res;
    }
    //Read data command and put them in structure
    while (counter < argc) {
        if (strcmp(argv_copy[counter], "-max_files") == 0) {
            if (counter + 1 == argc) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
            uint32_t max_files = atouint32(argv_copy[counter + 1]);
            if (max_files == 0) {
                return ERR_NOT_ENOUGH_ARGUMENTS;
            }
            if (max_files == MAX_MAX_FILES) {
                return ERR_MAX_FILES;
            }
            imgfs_file.header.max_files = max_files;
            counter += 2;
        } else {
            if (strcmp(argv_copy[counter], "-thumb_res") == 0) {

                if (counter + 2 >= argc) {
                    return ERR_NOT_ENOUGH_ARGUMENTS;
                }
                uint16_t xy[] = {atouint16(argv_copy[counter + 1]),
                                 atouint16(argv_copy[counter + 2])
                                };
                int error;
                for (int i = 0; i < 2; ++i) {
                    error = verify_and_put_resolution(xy[i], &(imgfs_file.header.resized_res[i]), MAX_THUMB_RES);
                    if (error != ERR_NONE)return error;
                }
                counter += 3;
            } else {
                if (strcmp(argv_copy[counter], "-small_res") == 0) {
                    if (counter + 2 >= argc) {
                        return ERR_NOT_ENOUGH_ARGUMENTS;
                    }
                    uint16_t xy[] = {atouint16(argv_copy[counter + 1]),
                                     atouint16(argv_copy[counter + 2])
                                    };
                    int error;
                    for (int i = 0; i < 2; ++i) {
                        error = verify_and_put_resolution(xy[i], &(imgfs_file.header.resized_res[2 + i]),
                                                          MAX_SMALL_RES);
                        if (error != ERR_NONE)return error;
                    }
                    counter += 3;
                } else {
                    return ERR_INVALID_ARGUMENT;
                }
            }
        }
    }
    //Create the imgFS
    int err_create = do_create(filename, &imgfs_file);
    fclose(imgfs_file.file);
    free(imgfs_file.metadata);
    imgfs_file.metadata = NULL;
    return err_create;
}

/**********************************************************************
 * Deletes an image from the imgFS.
 */
int do_delete_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc < 2) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    if (argc > 2) {
        return ERR_INVALID_COMMAND;
    }

    if (strlen(argv[1]) == 0 || strlen(argv[1]) > MAX_IMG_ID) {
        return ERR_INVALID_IMGID;
    }

    struct imgfs_file img_file;
    int err_code = do_open(argv[0], "r+b", &img_file);
    if (err_code != ERR_NONE) {
        return err_code;
    }

    err_code = do_delete(argv[1], &img_file);
    if (err_code != ERR_NONE) {
        do_close(&img_file);
        return err_code;
    }

    do_close(&img_file);
    return ERR_NONE;
}

/**********************************************************************
 * Reads an image from the imgFS.
 */

int do_read_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 2 && argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char * const img_id = argv[1];

    const int resolution = (argc == 3) ? resolution_atoi(argv[2]) : ORIG_RES;
    if (resolution == -1) return ERR_RESOLUTIONS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "r+b", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size = 0;
    error = do_read(img_id, resolution, &image_buffer, &image_size, &myfile);
    do_close(&myfile);
    if (error != ERR_NONE) {
        return error;
    }

    // Extracting to a separate image file.
    char* tmp_name = NULL;
    create_name(img_id, resolution, &tmp_name);
    if (tmp_name == NULL) return ERR_OUT_OF_MEMORY;
    error = write_disk_image(tmp_name, image_buffer, image_size);
    free(tmp_name);
    free(image_buffer);

    return error;
}

/**********************************************************************
 * * Inserts an image into the imgFS.
 */
int do_insert_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "r+b", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size;

    // Reads image from the disk.
    error = read_disk_image (argv[2], &image_buffer, &image_size);
    if (error != ERR_NONE) {
        do_close(&myfile);
        return error;
    }

    error = do_insert(image_buffer, image_size, argv[1], &myfile);
    free(image_buffer);
    do_close(&myfile);
    return error;
}

/********************************************************************
 * Verifies and puts the resolution.
 *******************************************************************/
int verify_and_put_resolution(uint16_t new_data, uint16_t* emplacement, uint16_t max_resolution)
{
    M_REQUIRE_NON_NULL(emplacement);

    if (new_data == 0) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }
    if (new_data > max_resolution) {
        return ERR_RESOLUTIONS;
    }
    *emplacement = new_data;
    return ERR_NONE;
}

/********************************************************************
 * Creates the name of the file.
 *******************************************************************/
static void create_name(const char* img_id, int resolution, char** new_name)
{
    if(img_id == NULL)return;

    const char * suffix=NULL;
    switch (resolution) {
    case  ORIG_RES:
        suffix="orig";
        break;
    case  SMALL_RES:
        suffix="small";
        break;
    case  THUMB_RES:
        suffix="thumb";
        break;
    default:
        return;
    }
    size_t img_id_len= strlen(img_id);
    size_t suffix_len= strlen(suffix);

    const size_t SIZE_EXTINCTION = 5; //size of "_.jpg" that is fixed from beginning
    size_t name_len= img_id_len + suffix_len + SIZE_EXTINCTION;
    *new_name= malloc(name_len +1);
    if (*new_name== NULL) {
        free(*new_name);
        return;
    }

    sprintf(*new_name,"%s_%s.jpg",img_id,suffix);
}

/********************************************************************
 * Writes the image buffer and image size in the file.
 *******************************************************************/
static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size)
{
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(image_buffer);
    if(image_size<=0)return ERR_INVALID_ARGUMENT;

    FILE * file = fopen(filename,"wb");
    if (file==NULL) {
        return ERR_IO;
    }

    if(fwrite(image_buffer, image_size,1,file)!=1) {
        fclose(file);
        return ERR_IO;
    }

    fclose(file);
    return ERR_NONE;
}

/********************************************************************
 * Verifies and puts the resolution.
 *******************************************************************/
static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size)
{
    M_REQUIRE_NON_NULL(path);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);

    FILE * file = fopen(path,"rb");
    if (file==NULL) {
        return ERR_IO;
    }

    fseek(file,0,SEEK_END);
    long size= ftell(file);
    fseek(file,0,SEEK_SET);

    *image_buffer= malloc((unsigned long) size);
    if (*image_buffer==NULL) {
        fclose(file);
        return ERR_OUT_OF_MEMORY;
    }

    if (fread(*image_buffer, sizeof (char ), (unsigned long) size, file) != (size_t)size) {
        fclose(file);
        free(*image_buffer);
        return ERR_IO;
    }

    *image_size=(uint32_t) size;
    fclose(file);
    return ERR_NONE;
}


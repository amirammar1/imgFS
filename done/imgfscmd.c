/**
 * @file imgfscmd.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * Image Filesystem Command Line Tool
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused
#include <vips/vips.h>

#include <stdlib.h>
#include <string.h>

#define SIZE_COMMANDS 6

typedef int (*command)(int, char **);

struct command_mapping {
    char const name[10];
    command command;
};

struct command_mapping commands[] = {
    {"list",   do_list_cmd},
    {"create", do_create_cmd},
    {"help",   help},
    {"delete", do_delete_cmd},
    {"insert",do_insert_cmd},
    {"read",do_read_cmd}
};


/*******************************************************************************
 * MAIN
 */
int main(int argc, char *argv[])
{
    int ret = VIPS_INIT(argv[0]);
    if(ret<0) {
        return ERR_IMGLIB;
    }

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {

        argc--;
        argv++; // skips command call name

        //Shift to the second command
        int argc2 = argc - 1;
        char **argv2 = argv + 1;

        ret = ERR_INVALID_COMMAND;

        //Find the corresponding function
        for (int i = 0; i < SIZE_COMMANDS; ++i) {
            if (0 == strcmp(argv[0], commands[i].name)) {
                ret = (*commands[i].command)(argc2, argv2);
                break;
            }
        }
    }
    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc, argv);
    }
    vips_shutdown();
    return ret;
}

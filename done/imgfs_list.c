#include <stdio.h>
#include <string.h>
#include "imgfs.h"
#include "util.h"
#include "json-c/json.h"

/********************************************************************//**
 * Displays imgFS metadata.
 ********************************************************************** */
int do_list(const struct imgfs_file *imgfs_file, enum do_list_mode output_mode, char **json)
{


    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    switch (output_mode) {
    case STDOUT:
        // Print header
        print_header(&(imgfs_file->header));

        // Check if the database contains any images
        if (imgfs_file->header.nb_files == 0) {
            printf("<< empty imgFS >>\n");
            return ERR_NONE;
        }

        // Print metadata of each valid image
        for (uint32_t i = 0; i < imgfs_file->header.max_files; ++i) {
            if (imgfs_file->metadata[i].is_valid == NON_EMPTY) {
                print_metadata(&(imgfs_file->metadata[i]));
            }
        }
        break;
    case JSON : {

        struct json_object *json_obj = json_object_new_object();
        if (json_obj==NULL) {
            return ERR_RUNTIME;
        }

        struct json_object *json_array = json_object_new_array();
        if (json_array==NULL) {
            json_object_put(json_obj);
            return ERR_RUNTIME;
        }

        for (uint32_t i = 0; i < imgfs_file->header.max_files; ++i) {
            if (imgfs_file->metadata[i].is_valid == NON_EMPTY) {
                struct json_object *json_img_id = json_object_new_string(imgfs_file->metadata[i].img_id);
                if(json_img_id==NULL) {
                    json_object_put(json_obj);
                    json_object_put(json_array);
                    return ERR_RUNTIME;
                }
                json_object_array_add(json_array,json_img_id);
            }
        }

        json_object_object_add(json_obj,"Images",json_array);

        const char *json_str = json_object_to_json_string(json_obj);
        if (json_str==NULL) {
            json_object_put(json_obj);
            return ERR_RUNTIME;
        }

        *json= strdup(json_str);//used to allocate a copy of the string returned by json_object_to_json_string()
        if(*json==NULL) {
            json_object_put(json_obj);
            return ERR_RUNTIME;
        }

        json_object_put(json_obj);
        break;
    }

    default:
        break;
    }

    return ERR_NONE;
}

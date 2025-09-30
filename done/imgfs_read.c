#include "imgfs.h"
#include "error.h"
#include "image_content.h"
#include <stdlib.h>
#include <string.h>

/********************************************************************//**
 * Reads the content of an image from a imgFS.
 ********************************************************************** */
int do_read(const char* img_id, int resolution, char** image_buffer,
            uint32_t* image_size, struct imgfs_file* imgfs_file)
{

    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);
    M_REQUIRE_NON_NULL(imgfs_file);

    if(imgfs_file->header.nb_files == 0)return ERR_IMAGE_NOT_FOUND;

    int index=-1;
    for (int i = 0; i < (int) imgfs_file->header.max_files; ++i) {
        if (imgfs_file->metadata[i].is_valid == NON_EMPTY && strcmp(img_id,imgfs_file->metadata[i].img_id)==0) {
            index=i;
            break;
        }
    }
    if (index==-1) {
        return ERR_IMAGE_NOT_FOUND;
    }

    if (imgfs_file->metadata[index].offset[resolution]==0 || imgfs_file->metadata[index].size[resolution]==0) {
        if (resolution==ORIG_RES)return ERR_IMAGE_NOT_FOUND;
        int err_resize= lazily_resize(resolution,imgfs_file,(size_t) index);
        if (err_resize!=ERR_NONE) {
            return err_resize;
        }
    }


    fseek(imgfs_file->file, (long) imgfs_file->metadata[index].offset[resolution], SEEK_SET);

    *image_buffer= calloc(1,imgfs_file->metadata[index].size[resolution]);
    if (*image_buffer==NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    size_t read= fread(*image_buffer,imgfs_file->metadata[index].size[resolution],1,imgfs_file->file);
    if (read!=1) {
        free(*image_buffer);
        return ERR_IO;
    }


    *image_size=imgfs_file->metadata[index].size[resolution];

    return ERR_NONE;
}

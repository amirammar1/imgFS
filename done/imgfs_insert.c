#include "imgfs.h"
#include "image_dedup.h"
#include "error.h"
#include "image_content.h"
#include <openssl/sha.h>
#include <string.h>


/********************************************************************//**
 * Insert image in the imgFS file
 ********************************************************************** */
int do_insert(const char* image_buffer, size_t image_size,
              const char* img_id, struct imgfs_file* imgfs_file)
{

    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);


    if (imgfs_file->header.nb_files >= imgfs_file->header.max_files) {
        return ERR_IMGFS_FULL;
    }

    //find the empty entry
    size_t empty_entry= SIZE_MAX;
    for (size_t i = 0; i <imgfs_file->header.max_files ; ++i) {
        if (imgfs_file->metadata[i].is_valid==EMPTY) {
            empty_entry=i;
            break;
        }
    }

    //calculate sha and place it in the metadata
    unsigned char sha256[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)image_buffer,image_size,sha256);
    memcpy(imgfs_file->metadata[empty_entry].SHA,sha256,SHA256_DIGEST_LENGTH);

    //store id
    strncpy(imgfs_file->metadata[empty_entry].img_id,img_id,MAX_IMG_ID);

    //store the img size
    imgfs_file->metadata[empty_entry].size[ORIG_RES]= (uint32_t) image_size;

    //store the width and height
    uint32_t width, height;
    int resolution_error= get_resolution( &height,&width,image_buffer,image_size);
    if (resolution_error!= ERR_NONE) {
        return resolution_error;
    }
    imgfs_file->metadata[empty_entry].orig_res[0]= width;
    imgfs_file->metadata[empty_entry].orig_res[1]= height;

    //de_deup image
    int  dedup_error= do_name_and_content_dedup(imgfs_file,(uint32_t) empty_entry);
    if (dedup_error!=ERR_NONE) {
        return dedup_error;
    }

    if (imgfs_file->metadata[empty_entry].offset[ORIG_RES]==0) {
        fseek(imgfs_file->file,0,SEEK_END);
        imgfs_file->metadata[empty_entry].offset[ORIG_RES] = (uint64_t) ftell(imgfs_file->file);
        fwrite(image_buffer,image_size,1,imgfs_file->file);
    }


    imgfs_file->metadata[empty_entry].is_valid=NON_EMPTY;
    imgfs_file->metadata[empty_entry].offset[THUMB_RES]=0;
    imgfs_file->metadata[empty_entry].offset[SMALL_RES]=0;
    imgfs_file->metadata[empty_entry].size[THUMB_RES]=0;
    imgfs_file->metadata[empty_entry].size[SMALL_RES]=0;
    imgfs_file->header.nb_files++;
    imgfs_file->header.version++;


    //write header to disk
    fseek(imgfs_file->file,0,SEEK_SET);
    size_t err_write= fwrite(&(imgfs_file->header), sizeof(struct imgfs_header),1,imgfs_file->file);
    if (err_write!=1) {
        return ERR_IO;
    }

    //write metadata to disk
    fseek(imgfs_file->file, (long) (sizeof(struct imgfs_header) + empty_entry * sizeof(struct img_metadata)), SEEK_SET);
    err_write= fwrite(&(imgfs_file->metadata[empty_entry]), sizeof(struct img_metadata),1,imgfs_file->file);
    if (err_write!=1) {
        return ERR_IO;
    }


    return ERR_NONE;
}

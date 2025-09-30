#include "imgfs.h"

#include "util.h"   // for _unused
#include <openssl/sha.h>
#include <string.h>


/*******************************************************************
 * remove deduplicate
 */
int do_name_and_content_dedup(struct imgfs_file *imgfs_file, uint32_t index)
{

    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(imgfs_file->file);

    if (index >= imgfs_file->header.max_files) {
        return ERR_IMAGE_NOT_FOUND;
    }

    // Get the SHA of the image at the specified index
    unsigned char *sha_of_image_index = imgfs_file->metadata[index].SHA;

    // Iterate through all images in the file system
    for (uint32_t i = 0; i < imgfs_file->header.max_files; ++i) {
        // Skip the current image and empty slots
        if (index == i || imgfs_file->metadata[i].is_valid == EMPTY) {
            continue;
        }

        // Check if the image ID is the same (duplicate name)
        if (strcmp(imgfs_file->metadata[i].img_id, imgfs_file->metadata[index].img_id) == 0) {
            return ERR_DUPLICATE_ID;
        }

        // Check if the content hash matches
        if (memcmp(imgfs_file->metadata[i].SHA, sha_of_image_index, SHA256_DIGEST_LENGTH) == 0) {
            // Update the metadata of the original image to include all resolutions of the duplicate image
            for (int j = THUMB_RES; j <= ORIG_RES; ++j) {
                imgfs_file->metadata[index].offset[j] = imgfs_file->metadata[i].offset[j];
                imgfs_file->metadata[index].size[j] = imgfs_file->metadata[i].size[j];
            }
            return ERR_NONE;
        }
    }

    // If no duplicates found based on content hash, reset original resolution offset
    imgfs_file->metadata[index].offset[ORIG_RES] = 0;

    return ERR_NONE;
}

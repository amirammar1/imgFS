#include "imgfs.h"
#include "error.h"
#include <stdio.h>
#include <string.h>

int do_delete(const char *img_id, struct imgfs_file *imgfs_file)
{

    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    //Finding the image
    int index = -1;
    for (int i = 0; i < (int) imgfs_file->header.max_files; i++) {
        if (strcmp((imgfs_file->metadata[i]).img_id, img_id) == 0) {
            index = i;
            break;
        }
    }
    if (index == -1 || (imgfs_file->metadata[index].is_valid == EMPTY)) {
        return ERR_IMAGE_NOT_FOUND;
    }

    // Invalidate the reference(how to rewrite the entire struct efficiently)
    imgfs_file->metadata[index].is_valid = EMPTY;

    // Adjust the header(+the changes in the disk)
    size_t location_deleted_image = sizeof(struct imgfs_header) + ((unsigned long) index) * sizeof(struct img_metadata);
    if (fseek(imgfs_file->file, (long) location_deleted_image, SEEK_SET) != 0) {
        return ERR_IO;
    }
    if (fwrite(&(imgfs_file->metadata[index]), sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }
    imgfs_file->header.nb_files--;
    imgfs_file->header.version++;
    if (fseek(imgfs_file->file, 0, SEEK_SET) != 0) {
        return ERR_IO;
    }
    if (fwrite(&(imgfs_file->header), sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    return ERR_NONE;
}

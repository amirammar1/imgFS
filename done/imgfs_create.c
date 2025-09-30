#include "imgfs.h"
#include "error.h"
#include <stdio.h>
#include <string.h>
#include "stdlib.h"

int do_create(const char *imgfs_filename, struct imgfs_file *imgfs_file)
{

    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_filename);

    // Initialize the binary file
    FILE *file = fopen(imgfs_filename, "wb");
    if (file == NULL) {
        return ERR_IO;
    }
    imgfs_file->file = file;

    // Initialize the header structure
    strncpy(imgfs_file->header.name, CAT_TXT, MAX_IMGFS_NAME);
    imgfs_file->header.version = 0;
    imgfs_file->header.nb_files = 0;
    imgfs_file->header.unused_32 = 0;
    imgfs_file->header.unused_64 = 0;

    //Allocate memory for the metadata
    struct img_metadata *metadata = calloc(imgfs_file->header.max_files, sizeof(struct img_metadata));
    if (metadata == NULL) {
        fclose(imgfs_file->file);
        return ERR_OUT_OF_MEMORY;
    }
    imgfs_file->metadata = metadata;

    //Initialize the metadata structure
    for (uint32_t i = 0; i < imgfs_file->header.max_files; ++i) {
        imgfs_file->metadata[i].is_valid = EMPTY;
    }

    //Write the header
    size_t check_written_head = fwrite(&(imgfs_file->header), sizeof(struct imgfs_header), 1, imgfs_file->file);
    if (check_written_head != 1) {
        free(imgfs_file->metadata);
        imgfs_file->metadata = NULL;
        fclose(imgfs_file->file);
        return ERR_IO;
    }

    //Write the metadata
    size_t check_written_metadata = fwrite(imgfs_file->metadata, sizeof(struct img_metadata),
                                           imgfs_file->header.max_files, imgfs_file->file);


    if (check_written_metadata != imgfs_file->header.max_files) {
        free(imgfs_file->metadata);
        imgfs_file->metadata = NULL;
        return ERR_IO;
    }
    // Display the nb of items written
    printf("%u item(s) written\n", imgfs_file->header.max_files + 1);

    return ERR_NONE;
}

#include "imgfs.h"
#include "error.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <string.h>
#include <vips/vips.h>

/*******************************************************************
 * resize Image
 */
int lazily_resize(int resolution, struct imgfs_file* imgfs_file, size_t index)
{
    //Verify valid parameters
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);
    M_REQUIRE_NON_NULL(imgfs_file->file);
    if (resolution != THUMB_RES && resolution != SMALL_RES && resolution != ORIG_RES) {
        return ERR_RESOLUTIONS;
    }
    if(index>= imgfs_file->header.max_files) {
        return ERR_INVALID_IMGID;
    }
    if (imgfs_file->metadata[index].is_valid == EMPTY) {
        return ERR_INVALID_IMGID;
    }
    if (imgfs_file->metadata[index].size[resolution] != 0) {
        return ERR_NONE;
    }
    if (resolution == ORIG_RES) {
        return ERR_NONE;
    }


    //Prepare the output_buffer that contain original image
    char* orig_buf = malloc(imgfs_file->metadata[index].size[ORIG_RES]);
    if (orig_buf == NULL) {
        return ERR_IO;
    }
    if(fseek(imgfs_file->file, (long) imgfs_file->metadata[index].offset[ORIG_RES], SEEK_SET)!=0) {
        return ERR_IO;
    }
    if (fread(orig_buf,imgfs_file->metadata[index].size[ORIG_RES],1,imgfs_file->file) !=1) {
        return ERR_IO;
    }

    //Load original image
    VipsImage *image_initial = NULL;
    int err_vips = vips_jpegload_buffer(orig_buf, imgfs_file->metadata[index].size[ORIG_RES], &image_initial, NULL);
    if (err_vips != 0) {
        return ERR_IMGLIB;
    }

    //Resize the image
    VipsImage *image_resize = NULL;
    if (vips_thumbnail_image(image_initial, &image_resize, imgfs_file->header.resized_res[2 * resolution],NULL) != 0) {
        free(orig_buf);
        g_object_unref(VIPS_OBJECT(image_initial));
        image_initial = NULL;
        return ERR_IO;
    }

    //Save the image and it's size in corresponding variables
    size_t size_thumb;
    char *output_buffer = NULL;
    err_vips = vips_jpegsave_buffer(image_resize, &output_buffer, &size_thumb, NULL);
    free(orig_buf);
    g_object_unref(VIPS_OBJECT(image_initial));
    image_initial = NULL;
    g_object_unref(VIPS_OBJECT(image_resize));
    image_resize = NULL;
    if (err_vips!=0) {
        return ERR_IO;
    }

    //write the copy of image at the end of file
    fseek(imgfs_file->file, 0, SEEK_END);
    uint64_t new_offset = (uint64_t) ftell(imgfs_file->file);
    int check_write = (int) fwrite(output_buffer, size_thumb, 1, imgfs_file->file);
    free(output_buffer);
    output_buffer = NULL;
    if (check_write!=1) {
        return ERR_IO;
    }

    //Update metadata in struct
    imgfs_file->metadata[index].size[resolution] = (uint32_t) size_thumb;
    imgfs_file->metadata[index].offset[resolution] = new_offset;

    //Update metadata on disk
    if(fseek(imgfs_file->file, (long) (sizeof(imgfs_file->header) + index * sizeof(struct img_metadata)), SEEK_SET)!=0) {
        return ERR_IO;
    }
    if (fwrite(&imgfs_file->metadata[index], sizeof(struct img_metadata), 1, imgfs_file->file) !=1) {
        return ERR_IO;
    }
    return ERR_NONE;
}


/*******************************************************************
 * retrieve resolution
 */
int get_resolution(uint32_t *height, uint32_t *width,
                   const char *image_buffer, size_t image_size)
{
    M_REQUIRE_NON_NULL(height);
    M_REQUIRE_NON_NULL(width);
    M_REQUIRE_NON_NULL(image_buffer);

    VipsImage* original = NULL;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    const int err = vips_jpegload_buffer((void*) image_buffer, image_size,
                                         &original, NULL);
#pragma GCC diagnostic pop
    if (err != ERR_NONE) return ERR_IMGLIB;

    *height = (uint32_t) vips_image_get_height(original);
    *width  = (uint32_t) vips_image_get_width (original);

    g_object_unref(VIPS_OBJECT(original));
    return ERR_NONE;
}

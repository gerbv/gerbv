#include <stdlib.h>
#include <gerbv.h>

int main(void) {
    gerbv_project_t *project = gerbv_create_project();
    if (!project)
        return EXIT_FAILURE;
    gerbv_destroy_project(project);

    /* Force linker to resolve DXF export symbol — catches
       missing dxflib objects in libgerbv (issue #366).
       Cast between function pointer types is defined by C;
       casting to void* is not (UB outside POSIX). */
    void (*volatile dxf_check)(void) =
        (void (*)(void))gerbv_export_dxf_file_from_image;
    (void)dxf_check;

    return EXIT_SUCCESS;
}

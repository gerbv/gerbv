/*
 * test_x2_attrs.c — Verify X2/X3 attribute parsing, storage, and round-trip.
 *
 * Exit 0 on success, non-zero on failure.
 * Each assertion prints a description before checking so failures are obvious.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gerbv.h"

static int failures = 0;

#define CHECK(desc, cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", desc); \
        failures++; \
    } else { \
        printf("  ok: %s\n", desc); \
    } \
} while (0)

#define CHECK_STR(desc, actual, expected) do { \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if (_a == NULL || strcmp(_a, _e) != 0) { \
        fprintf(stderr, "FAIL: %s  (got \"%s\", expected \"%s\")\n", \
                desc, _a ? _a : "(null)", _e); \
        failures++; \
    } else { \
        printf("  ok: %s\n", desc); \
    } \
} while (0)

static void check_null(const char *desc, const void *ptr)
{
    if (ptr != NULL) {
        fprintf(stderr, "FAIL: %s  (expected NULL, got non-NULL)\n", desc);
        failures++;
    } else {
        printf("  ok: %s\n", desc);
    }
}

/*
 * Find a net by its stop coordinates (in mm, converted to internal inches).
 * Returns the first match after the sentinel net.
 */
static gerbv_net_t *
find_flash_net(gerbv_image_t *image, double stop_x_mm, double stop_y_mm)
{
    double sx = stop_x_mm / 25.4;
    double sy = stop_y_mm / 25.4;
    gerbv_net_t *net;

    for (net = image->netlist->next; net != NULL; net = net->next) {
        if (net->aperture_state == GERBV_APERTURE_STATE_FLASH) {
            double dx = net->stop_x - sx;
            double dy = net->stop_y - sy;
            if (dx * dx + dy * dy < 1e-6)
                return net;
        }
    }
    return NULL;
}

static void
test_parse_attributes(const char *input_path)
{
    printf("\n=== Phase 1: Parse and verify attribute storage ===\n");

    gerbv_image_t *image = gerbv_create_rs274x_image_from_filename(input_path);
    CHECK("Image parsed successfully", image != NULL);
    if (!image) return;

    /* --- File attributes (TF) --- */
    const gerbv_x2_attr_t *file_attrs = gerbv_image_get_x2_file_attrs(image);
    CHECK("File attributes exist", file_attrs != NULL);

    CHECK_STR("TF.GenerationSoftware first value",
              gerbv_x2_attr_get_value(file_attrs, ".GenerationSoftware"),
              "KiCad");

    const gerbv_x2_attr_t *gen = gerbv_x2_attr_find(file_attrs, ".GenerationSoftware");
    CHECK("TF.GenerationSoftware has 3 values", gen != NULL && gen->n_values == 3);

    CHECK_STR("TF.FileFunction first value",
              gerbv_x2_attr_get_value(file_attrs, ".FileFunction"),
              "Copper");

    const gerbv_x2_attr_t *ff = gerbv_x2_attr_find(file_attrs, ".FileFunction");
    CHECK("TF.FileFunction has 3 values", ff != NULL && ff->n_values == 3);
    if (ff && ff->n_values >= 3)
        CHECK_STR("TF.FileFunction third value", ff->values[2], "Top");

    CHECK_STR("TF.FilePolarity",
              gerbv_x2_attr_get_value(file_attrs, ".FilePolarity"),
              "Positive");

    CHECK_STR("TF.CreationDate",
              gerbv_x2_attr_get_value(file_attrs, ".CreationDate"),
              "2024-01-01T00:00:00+00:00");

    /* --- Aperture attributes (TA) --- */
    /* D10 (index 10): should have .AperFunction = SMDPad,CuDef */
    CHECK("Aperture D10 exists", image->aperture[10] != NULL);
    if (image->aperture[10]) {
        const gerbv_x2_attr_t *ap10 = gerbv_aperture_get_x2_attrs(image->aperture[10]);
        CHECK("D10 has aperture attributes", ap10 != NULL);
        CHECK_STR("D10 .AperFunction first value",
                  gerbv_x2_attr_get_value(ap10, ".AperFunction"), "SMDPad");
        const gerbv_x2_attr_t *af10 = gerbv_x2_attr_find(ap10, ".AperFunction");
        CHECK("D10 .AperFunction has 2 values", af10 != NULL && af10->n_values == 2);
        if (af10 && af10->n_values >= 2)
            CHECK_STR("D10 .AperFunction second value", af10->values[1], "CuDef");
    }

    /* D11 (index 11): should have .AperFunction = ComponentPad */
    CHECK("Aperture D11 exists", image->aperture[11] != NULL);
    if (image->aperture[11]) {
        const gerbv_x2_attr_t *ap11 = gerbv_aperture_get_x2_attrs(image->aperture[11]);
        CHECK("D11 has aperture attributes", ap11 != NULL);
        CHECK_STR("D11 .AperFunction",
                  gerbv_x2_attr_get_value(ap11, ".AperFunction"), "ComponentPad");
    }

    /* D12 (index 12): should have NO attributes (TD cleared before AD) */
    CHECK("Aperture D12 exists", image->aperture[12] != NULL);
    if (image->aperture[12]) {
        check_null("D12 has no aperture attributes",
                   gerbv_aperture_get_x2_attrs(image->aperture[12]));
    }

    /* --- Object attributes (TO) on nets --- */
    /* Flash at X=100mm Y=100mm: R42, VCC, R42 pin 1 */
    gerbv_net_t *net1 = find_flash_net(image, 100.0, 100.0);
    CHECK("Flash net at (100,100) found", net1 != NULL);
    if (net1) {
        const gerbv_x2_attr_t *attrs1 = gerbv_net_get_x2_attrs(net1);
        CHECK("Net1 has object attributes", attrs1 != NULL);
        CHECK_STR("Net1 .C = R42", gerbv_x2_attr_get_value(attrs1, ".C"), "R42");
        CHECK_STR("Net1 .N = VCC", gerbv_x2_attr_get_value(attrs1, ".N"), "VCC");
        CHECK_STR("Net1 .P first value = R42",
                  gerbv_x2_attr_get_value(attrs1, ".P"), "R42");
        const gerbv_x2_attr_t *p1 = gerbv_x2_attr_find(attrs1, ".P");
        CHECK("Net1 .P has 2 values", p1 != NULL && p1->n_values == 2);
        if (p1 && p1->n_values >= 2)
            CHECK_STR("Net1 .P pin = 1", p1->values[1], "1");
    }

    /* Flash at X=100.5mm Y=100mm: R42, GND, R42 pin 2 */
    gerbv_net_t *net2 = find_flash_net(image, 100.5, 100.0);
    CHECK("Flash net at (100.5,100) found", net2 != NULL);
    if (net2) {
        const gerbv_x2_attr_t *attrs2 = gerbv_net_get_x2_attrs(net2);
        CHECK("Net2 has object attributes", attrs2 != NULL);
        CHECK_STR("Net2 .C = R42", gerbv_x2_attr_get_value(attrs2, ".C"), "R42");
        CHECK_STR("Net2 .N = GND", gerbv_x2_attr_get_value(attrs2, ".N"), "GND");
        const gerbv_x2_attr_t *p2 = gerbv_x2_attr_find(attrs2, ".P");
        CHECK("Net2 .P has 2 values", p2 != NULL && p2->n_values == 2);
        if (p2 && p2->n_values >= 2)
            CHECK_STR("Net2 .P pin = 2", p2->values[1], "2");
    }

    /* Flash at X=101mm Y=100mm: C1, CLK13, C1 pin 1 */
    gerbv_net_t *net3 = find_flash_net(image, 101.0, 100.0);
    CHECK("Flash net at (101,100) found", net3 != NULL);
    if (net3) {
        const gerbv_x2_attr_t *attrs3 = gerbv_net_get_x2_attrs(net3);
        CHECK("Net3 has object attributes", attrs3 != NULL);
        CHECK_STR("Net3 .C = C1", gerbv_x2_attr_get_value(attrs3, ".C"), "C1");
        CHECK_STR("Net3 .N = CLK13", gerbv_x2_attr_get_value(attrs3, ".N"), "CLK13");
    }

    /* Line nets after TD clear should have no object attributes */
    gerbv_net_t *net;
    gboolean found_line_without_attrs = FALSE;
    for (net = image->netlist->next; net != NULL; net = net->next) {
        if (net->aperture_state == GERBV_APERTURE_STATE_ON &&
            net->x2_attrs == NULL) {
            found_line_without_attrs = TRUE;
            break;
        }
    }
    CHECK("Line nets after TD have no object attributes", found_line_without_attrs);

    gerbv_destroy_image(image);
}

static void
test_round_trip(const char *input_path, const char *tmp_dir)
{
    printf("\n=== Phase 2: Round-trip (parse -> export -> parse) ===\n");

    gerbv_image_t *image1 = gerbv_create_rs274x_image_from_filename(input_path);
    CHECK("Original image parsed", image1 != NULL);
    if (!image1) return;

    /* Export to temp file */
    char export_path[512];
    snprintf(export_path, sizeof(export_path), "%s/roundtrip.gbx", tmp_dir);

    gboolean exported = gerbv_export_rs274x_file_from_image(export_path, image1, NULL);
    CHECK("Export succeeded", exported);

    /* Re-parse the exported file */
    gerbv_image_t *image2 = gerbv_create_rs274x_image_from_filename(export_path);
    CHECK("Re-parsed image from export", image2 != NULL);
    if (!image2) {
        gerbv_destroy_image(image1);
        return;
    }

    /* Verify file attributes survived */
    const gerbv_x2_attr_t *fa1 = gerbv_image_get_x2_file_attrs(image1);
    const gerbv_x2_attr_t *fa2 = gerbv_image_get_x2_file_attrs(image2);

    CHECK_STR("Round-trip TF.GenerationSoftware",
              gerbv_x2_attr_get_value(fa2, ".GenerationSoftware"),
              gerbv_x2_attr_get_value(fa1, ".GenerationSoftware"));

    CHECK_STR("Round-trip TF.FileFunction",
              gerbv_x2_attr_get_value(fa2, ".FileFunction"),
              gerbv_x2_attr_get_value(fa1, ".FileFunction"));

    CHECK_STR("Round-trip TF.FilePolarity",
              gerbv_x2_attr_get_value(fa2, ".FilePolarity"),
              gerbv_x2_attr_get_value(fa1, ".FilePolarity"));

    /* Verify aperture attributes survived on at least one aperture */
    gboolean found_aper_attrs = FALSE;
    for (int i = 0; i < APERTURE_MAX; i++) {
        if (image2->aperture[i] && image2->aperture[i]->x2_attrs) {
            const char *af = gerbv_x2_attr_get_value(
                image2->aperture[i]->x2_attrs, ".AperFunction");
            if (af) {
                found_aper_attrs = TRUE;
                break;
            }
        }
    }
    CHECK("Round-trip preserved aperture attributes", found_aper_attrs);

    /* Verify object attributes survived on at least one net */
    gboolean found_obj_attrs = FALSE;
    gerbv_net_t *net;
    for (net = image2->netlist->next; net != NULL; net = net->next) {
        if (net->x2_attrs) {
            const char *comp = gerbv_x2_attr_get_value(net->x2_attrs, ".C");
            if (comp) {
                found_obj_attrs = TRUE;
                break;
            }
        }
    }
    CHECK("Round-trip preserved object attributes", found_obj_attrs);

    gerbv_destroy_image(image1);
    gerbv_destroy_image(image2);
}

int
main(int argc, char *argv[])
{
    const char *input_path;
    const char *tmp_dir;

    if (argc >= 2) {
        input_path = argv[1];
    } else {
        input_path = "inputs/test-gerber-x2-attributes.gbx";
    }

    if (argc >= 3) {
        tmp_dir = argv[2];
    } else {
        tmp_dir = "/tmp";
    }

    printf("Input: %s\n", input_path);
    printf("Temp:  %s\n", tmp_dir);

    test_parse_attributes(input_path);
    test_round_trip(input_path, tmp_dir);

    printf("\n=== Results: %d failure(s) ===\n", failures);
    return failures > 0 ? 1 : 0;
}

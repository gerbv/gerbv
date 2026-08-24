# GTK2 Deprecation in libgerbv

## Status

GTK2 is **end-of-life** as of GTK3 (December 2020). GTK4 is the current stable release.
libgerbv still depends on GTK2 for its GUI and uses `GdkColor` (16-bit RGB) in its public API.

## Impact on libgerbv

### API Surface Issues

1. **`gerbv.h:73`** — `#include <gtk/gtk.h>` pulls in all of GTK2, even for headless/library-only usage
2. **`GdkColor` in public structs** — `gerbv_fileinfo_t.color` and `gerbv_project_t.background` use `GdkColor` which:
   - Has no alpha channel (alpha is a separate `gushort` field on `gerbv_fileinfo_t`)
   - Uses 16-bit color values (0-65535) instead of normalized floats
   - Is deprecated in GTK3 in favor of `GdkRGBA` (float RGBA)
3. **`gerbv_render_layer_to_cairo_target()`** — the Cairo rendering path doesn't need GTK at all, but the header forces GTK2 linkage

### Compilation Warnings (GLib 2.88+)

```
gtktypeutils.h:236: 'GTypeDebugFlags' is deprecated
gtktooltips.h:73: 'GTimeVal' is deprecated: Use 'GDateTime' instead
```

These are harmless but noisy. Suppressed with `-w` in practice.

## Proposed Fix: Split libgerbv from GTK2

### Phase 1: Decouple header (non-breaking) — **Implemented**

Added `gerbv_color_t` standalone RGBA color type to `gerbv.h` (normalized floats 0.0–1.0):

```c
typedef struct {
    double red;
    double green;
    double blue;
    double alpha;
} gerbv_color_t;
```

Added backward-compatibility macros `GERBV_COLOR_FROM_GDK()` and `GERBV_COLOR_TO_GDK()`
for converting between `GdkColor` (16-bit) and `gerbv_color_t` (float).

Existing `GdkColor` fields in `gerbv_fileinfo_t` and `gerbv_project_t` are **unchanged**
(no ABI break). The new type and macros provide a migration path for Phase 2.

### Phase 2: Remove GTK include from gerbv.h

Move `#include <gtk/gtk.h>` into the GUI source files only (`src/main.c`, `src/callbacks.c`, etc.).
`gerbv.h` should only need `<glib.h>` (for `gboolean`, `gchar`, etc.) and `<cairo.h>`.

### Phase 3: Optional GTK-free build

Add a CMake/Meson option to build libgerbv without GTK:

```
-DGERBV_HEADLESS=ON  # Build libgerbv only, no GUI
```

This is the common use case for server-side rendering (e.g., Source Parts API).

## Workaround (current)

In `gerbv-render.c` (Parts Studio tools):

```c
/* GdkColor uses 16-bit values (0-65535), not floats */
project->file[f]->color.red = ((color >> 24) & 0xFF) * 257;
project->file[f]->color.green = ((color >> 16) & 0xFF) * 257;
project->file[f]->color.blue = ((color >> 8) & 0xFF) * 257;
project->file[f]->alpha = (gushort)(((color) & 0xFF) * 257);
```

Alpha is a separate field on `gerbv_fileinfo_t`, not part of `GdkColor`.

## References

- [GTK 2.x EOL announcement](https://blog.gtk.org/)
- [GdkRGBA documentation](https://docs.gtk.org/gdk3/struct.RGBA.html)
- [Cairo standalone usage](https://www.cairographics.org/)
- [gerbv upstream repo](https://github.com/gerbv/gerbv)
- [SourceParts fork](https://github.com/SourceParts/gerbv)

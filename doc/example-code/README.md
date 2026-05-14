# Using libgerbv from your own program

Gerbv is split into a GUI application and a reusable C library, **libgerbv**.
The library exposes parsing, editing, and rendering of Gerber (RS-274X), Excellon
drill, and pick-and-place files. The public header is `gerbv.h`.

This directory contains small, runnable examples:

| Example | What it shows |
|---------|---------------|
| `example1.c` | Parse a Gerber file and export it back to RS-274X |
| `example2.c` | Duplicate, offset, and merge two layers, then export RS-274X |
| `example3.c` | Parse two files, recolour one, and export a PNG render |
| `example4.c` | Walk the netlist, drop nets matching a rule, re-export |
| `example5.c` | Build an image programmatically (lines, arcs, rectangles) |
| `example6.c` | Embed a libgerbv render surface in a GTK window |

Full HTML API reference: <https://gerbv.github.io/doc/>

Sample Gerber, Excellon drill, and pick-and-place files from a variety of
CAD tools (Eagle, OrCAD, Protel, Mentor BoardStation, …) live in the
top-level [`example/`](../../example/) directory. They are useful as
real-world inputs when wiring up a consumer of libgerbv.

## Minimal parse + PNG render

```c
#include "gerbv.h"

int main(void) {
    gerbv_project_t *project = gerbv_create_project();

    gerbv_open_layer_from_filename(project, "input.gbr");
    if (project->file[0] == NULL) {
        g_error("failed to parse input.gbr");
    }

    /* 640x480 PNG, autoscaled to fit the layer extents */
    gerbv_export_png_file_from_project_autoscaled(
        project, 640, 480, "output.png");

    gerbv_destroy_project(project);
    return 0;
}
```

For full control over scale, translation, render type
(`GERBV_RENDER_TYPE_CAIRO_HIGH_QUALITY`, etc.) and per-layer transforms,
use `gerbv_export_png_file_from_project()` with a populated
`gerbv_render_info_t`. See `example3.c`.

## Building against libgerbv

### Linux and macOS (pkg-config)

After `cmake --install build` you can compile any of the examples directly:

```sh
gcc -Wall -g $(pkg-config --cflags libgerbv) example1.c \
    $(pkg-config --libs libgerbv) -o example1
```

If you installed to `/usr/local/` (the default) and `pkg-config` cannot
find `libgerbv`, point it at the install prefix:

```sh
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
```

### Windows — MSYS2 (native)

Build gerbv with the `msys2-ucrt64-gcc` preset (see `BUILD.md`) and install:

```sh
cmake --preset msys2-ucrt64-gcc
cmake --build --preset msys2-ucrt64-gcc-release
cmake --install build --prefix /ucrt64
```

You can then compile a consumer from an MSYS2 UCRT64 shell exactly as on
Linux:

```sh
gcc $(pkg-config --cflags libgerbv) example1.c \
    $(pkg-config --libs libgerbv) -o example1.exe
```

### Windows — Visual Studio 2022 / MSVC

libgerbv is a plain C library and is callable from MSVC C/C++, but gerbv
itself is built only with GCC (MinGW-w64 or MSYS2 UCRT64). The supported
path on MSVC is to **build the DLL with MinGW**, then **link to it from
your VS 2022 project** through a generated import library.

1. Build a release with MSYS2:

   ```sh
   cmake --preset msys2-ucrt64-gcc
   cmake --build --preset msys2-ucrt64-gcc-release
   ```

   The artefacts you need are `libgerbv-*.dll` plus the public headers
   under `<prefix>/include/gerbv-*/gerbv.h` and the transitive runtime
   DLLs (GLib, GTK 2, Cairo, libpng, zlib, intl, …). The CI workflow
   under `.github/workflows/ci.yaml` shows the exact set.

2. Generate an MSVC-style import library from the DLL. From a
   Developer Command Prompt for VS 2022:

   ```bat
   :: produce a .def listing the exported symbols
   gendef libgerbv-1.dll

   :: convert the .def to an MSVC .lib
   lib /def:libgerbv-1.def /machine:x64 /out:libgerbv.lib
   ```

   `gendef` ships with MSYS2 (`pacman -S mingw-w64-ucrt-x86_64-tools-git`).
   You can also use `dlltool -d libgerbv-1.def -l libgerbv.lib`.

3. In your VS 2022 project:

   - **Additional Include Directories**: the `gerbv-*` include folder
     and the matching GLib / GTK 2 / Cairo include folders.
   - **Additional Library Directories**: the folder containing
     `libgerbv.lib`.
   - **Additional Dependencies**: `libgerbv.lib`.
   - Copy `libgerbv-1.dll` and every transitive DLL into your output
     folder, or add their location to `PATH` at runtime.

4. From C++, include the header inside an `extern "C"` block:

   ```cpp
   extern "C" {
   #include <gerbv.h>
   }

   int main() {
       gerbv_project_t *p = gerbv_create_project();
       gerbv_open_layer_from_filename(p, "input.gbr");
       gerbv_export_png_file_from_project_autoscaled(p, 800, 600, "out.png");
       gerbv_destroy_project(p);
   }
   ```

`gerbv.h` pulls in GLib and GTK 2 headers. If you only need parsing
and PNG/PDF/SVG export — the typical "headless" use case — those
headers still resolve at compile time and the corresponding runtime
DLLs must still be present at load time. A fully GLib/GTK-free build
is not currently supported.

## Notes on thread and process safety

libgerbv is not thread-safe across a single `gerbv_project_t`. If you
need to process files in parallel, give each thread its own project.

`gerbv` should be treated like any other codec when consuming
untrusted input: sandbox it (separate process, restricted permissions)
rather than calling it directly from a long-running service. See the
"Security" section in the top-level `README.md`.

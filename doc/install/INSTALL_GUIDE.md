# Gerbv Installation Guide

> To build Gerbv from source instead of installing a pre-built package, see [BUILD.md](../../BUILD.md).

## Quick Installation

### 1. Install the Package

```bash
sudo dpkg -i _packages/gerbv_2.10.0-75_amd64.deb
```

### 2. Verify Installation

```bash
./verify_installation.sh
```

### 3. Run Gerbv

```bash
gerbv
```

Or find it in your application menu under **Development** → **Gerbv**.

---

## What Gets Installed

### Package Files (in `/opt/gerbv.github/`)

- **Binary**: `/opt/gerbv.github/bin/gerbv`
- **Library**: `/opt/gerbv.github/lib/libgerbv.so.1.9.0`
- **Headers**: `/opt/gerbv.github/include/gerbv/gerbv.h` (for development)
- **Desktop File**: `/opt/gerbv.github/share/applications/gerbv.desktop`
- **Icons**: `/opt/gerbv.github/share/icons/hicolor/` (various sizes)
- **Man Page**: `/opt/gerbv.github/share/man/man1/gerbv.1.gz`
- **Translations**: `/opt/gerbv.github/share/locale/` (Japanese, Russian)
- **Examples**: `/opt/gerbv.github/share/doc/gerbv/example/`

### System Integration (created by postinst script)

The installation automatically creates symlinks and configurations:

1. **Binary in PATH**:
   - Symlink: `/usr/bin/gerbv` → `/opt/gerbv.github/bin/gerbv`
   - Allows running `gerbv` from anywhere

2. **Application Menu**:
   - Symlink: `/usr/share/applications/gerbv.desktop`
   - Adds Gerbv to your application menu

3. **Icons**:
   - Symlinks in `/usr/share/icons/hicolor/*/apps/`
   - Enables menu icons and file associations

4. **Library Configuration**:
   - File: `/etc/ld.so.conf.d/gerbv.conf`
   - Contains: `/opt/gerbv.github/lib`
   - Allows the system to find libgerbv.so

5. **Desktop Database**:
   - Updates application menu cache
   - Updates icon cache

---

## Verification Checklist

After installation, the verification script checks:

- ✓ Package is installed
- ✓ Installation directory exists
- ✓ Binary is executable
- ✓ Symlink `/usr/bin/gerbv` exists and points correctly
- ✓ Binary is in PATH
- ✓ Library exists
- ✓ Library configuration in `/etc/ld.so.conf.d/gerbv.conf`
- ✓ Library is in ldconfig cache
- ✓ Desktop file symlink exists
- ✓ Icon symlinks exist
- ✓ GLib schemas exist and compiled
- ✓ Binary executes successfully
- ✓ Man page exists

---

## Troubleshooting

### "gerbv: command not found"

**Cause**: Symlink not created or PATH not updated.

**Fix**:
```bash
# Check if symlink exists
ls -l /usr/bin/gerbv

# If missing, create manually
sudo ln -sf /opt/gerbv.github/bin/gerbv /usr/bin/gerbv
```

### "error while loading shared libraries: libgerbv.so.1"

**Cause**: Library path not configured.

**Fix**:
```bash
# Create library configuration
echo "/opt/gerbv.github/lib" | sudo tee /etc/ld.so.conf.d/gerbv.conf

# Update library cache
sudo ldconfig

# Verify library is found
ldconfig -p | grep libgerbv
```

### Gerbv not in application menu

**Cause**: Desktop database not updated or desktop environment needs restart.

**Fix**:
```bash
# Update desktop database
sudo update-desktop-database /usr/share/applications

# Update icon cache
sudo gtk-update-icon-cache -f -t /usr/share/icons/hicolor

# Log out and log back in, or restart desktop environment
```

### Settings not persisting

**Cause**: GLib schemas not compiled.

**Fix**:
```bash
# Compile schemas
sudo glib-compile-schemas /opt/gerbv.github/share/glib-2.0/schemas/
```

---

## Uninstalling

To completely remove Gerbv:

```bash
sudo dpkg -r gerbv
```

The postrm script automatically:
- Removes all symlinks
- Removes library configuration
- Updates desktop and icon caches
- Leaves `/opt/gerbv.github/` directory (only package manager metadata removed)

To remove everything including the installation directory:

```bash
sudo dpkg -r gerbv
sudo rm -rf /opt/gerbv.github
```

---

## Usage Examples

### View a Gerber file

```bash
gerbv path/to/file.gbr
```

### View multiple layers

```bash
gerbv board_top.gbr board_bottom.gbr drill.cnc
```

### Export to PNG

```bash
gerbv -x png -o output.png board.gbr
```

### Export to PDF

```bash
gerbv -x pdf -o output.pdf layer1.gbr layer2.gbr
```

### Get help

```bash
gerbv --help
man gerbv
```

---

## Development Usage

If you're developing software that uses libgerbv:

### Compile with libgerbv

```bash
gcc myapp.c -o myapp $(pkg-config --cflags --libs libgerbv)
```

### Headers location

```c
#include <gerbv/gerbv.h>
```

### pkg-config file

```bash
pkg-config --cflags libgerbv
# Output: -I/opt/gerbv.github/include/gerbv

pkg-config --libs libgerbv
# Output: -L/opt/gerbv.github/lib -lgerbv
```

---

## Building Different Package Types

### Standard `/usr` installation (recommended for distribution)

```bash
cmake --preset linux-gnu-gcc
cmake --build build --config Release
cpack --preset deb -C Release
```

This creates a package that installs to `/usr/local/` (no integration scripts needed).

### Custom `/opt` installation (parallel installs)

```bash
cmake --preset linux-gnu-gcc-opt
cmake --build build --config Release
cpack --preset deb-opt -C Release
```

This creates a package with integration scripts for `/opt/gerbv.github/`.

---

## Support

- **Documentation**: https://gerbv.github.io/doc/
- **Issues**: https://github.com/gerbv/gerbv/issues
- **Mailing Lists**:
  - gerbv-devel@lists.sourceforge.net
  - geda-user@delorie.com

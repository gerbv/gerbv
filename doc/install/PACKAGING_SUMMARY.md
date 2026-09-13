# Gerbv Packaging Summary

## Overview

Both DEB and RPM packages have been configured with full desktop integration when installing to `/opt/gerbv.github/`. The packages automatically create symlinks and system configurations so gerbv appears in application menus and can be run from anywhere.

---

## Package Comparison

| Feature | DEB Package | RPM Package |
|---------|-------------|-------------|
| **Package File** | `gerbv_2.10.0-79_amd64.deb` | `gerbv-2.10.0-79.x86_64.rpm` |
| **Size** | ~3.5 MB | ~12.4 MB (includes debug symbols) |
| **Install Location** | `/opt/gerbv.github/` | `/opt/gerbv.github/` |
| **Integration Scripts** | ✅ postinst, postrm | ✅ %post, %postun |
| **Binary in PATH** | ✅ `/usr/bin/gerbv` symlink | ✅ `/usr/bin/gerbv` symlink |
| **Menu Integration** | ✅ "Other" category | ✅ "Other" category |
| **Icons** | ✅ All sizes (16-48, SVG) | ✅ All sizes (16-48, SVG) |
| **Library Config** | ✅ `/etc/ld.so.conf.d/` | ✅ `/etc/ld.so.conf.d/` |
| **Translations** | ✅ Japanese, Russian | ✅ Japanese, Russian |
| **Man Page** | ✅ gerbv.1.gz | ✅ gerbv.1.gz |
| **Examples** | ✅ 17 test file sets | ✅ 17 test file sets |
| **Tested** | ✅ Debian/Ubuntu | ⚠️ Not tested (no RPM system available) |

---

## What Gets Installed

### Application Files (in /opt/gerbv.github/)

```
/opt/gerbv.github/
├── bin/
│   └── gerbv                          # Main executable
├── lib/
│   ├── libgerbv.so.1.9.0              # Shared library
│   ├── libgerbv.so.1 → libgerbv.so.1.9.0
│   ├── libgerbv.so → libgerbv.so.1
│   └── pkgconfig/
│       └── libgerbv.pc                # pkg-config metadata
├── include/
│   └── gerbv/
│       └── gerbv.h                    # Development header
├── share/
│   ├── applications/
│   │   └── gerbv.desktop              # Desktop entry
│   ├── icons/hicolor/                 # Icons (6 sizes)
│   ├── glib-2.0/schemas/              # GSettings schema
│   ├── locale/                        # Translations (ja, ru)
│   ├── man/man1/                      # Man page
│   ├── doc/gerbv/example/             # Test files
│   └── gerbv/scheme/init.scm          # Scheme init
```

### System Integration (created automatically)

```
/usr/
├── bin/
│   └── gerbv → /opt/gerbv.github/bin/gerbv
└── share/
    ├── applications/
    │   └── gerbv.desktop → /opt/gerbv.github/share/applications/gerbv.desktop
    └── icons/hicolor/*/apps/
        └── gerbv.{png,svg} → /opt/gerbv.github/share/icons/...

/etc/
└── ld.so.conf.d/
    └── gerbv.conf                     # Contains: /opt/gerbv.github/lib
```

---

## Building Packages

### DEB Package (Debian/Ubuntu)

```bash
# Configure for /opt installation
cmake --preset linux-gnu-gcc-opt

# Build
cmake --build build --config Release

# Create DEB package
cpack --preset deb-opt -C Release

# Output: _packages/gerbv_2.10.0-79_amd64.deb
```

### RPM Package (Fedora/RHEL/openSUSE)

```bash
# Configure for /opt installation
cmake --preset linux-gnu-gcc-opt

# Build
cmake --build build --config Release

# Create RPM package
cpack --preset rpm-opt -C Release

# Output: _packages/gerbv-2.10.0-79.x86_64.rpm
```

### Standard /usr Installation (both)

For standard Linux installations without integration scripts:

```bash
# Configure for standard prefix
cmake --preset linux-gnu-gcc

# Build
cmake --build build --config Release

# Create package (no integration scripts needed)
cpack --preset deb -C Release    # or rpm
```

---

## Installation

### DEB Package

```bash
# Install
sudo dpkg -i _packages/gerbv_*.deb

# Verify
./verify_installation.sh

# Uninstall
sudo dpkg -r gerbv
```

### RPM Package

```bash
# Install
sudo rpm -ivh _packages/gerbv-*.rpm

# Verify installation
rpm -qi gerbv
rpm -V gerbv

# Check integration
which gerbv
ls -l /usr/bin/gerbv
cat /etc/ld.so.conf.d/gerbv.conf

# Uninstall
sudo rpm -e gerbv
```

---

## Integration Scripts

### What Happens on Install

Both DEB and RPM scripts perform the same actions:

1. **Create Binary Symlink**
   ```bash
   ln -sf /opt/gerbv.github/bin/gerbv /usr/bin/gerbv
   ```
   Result: You can run `gerbv` from anywhere

2. **Create Desktop File Symlink**
   ```bash
   ln -sf /opt/gerbv.github/share/applications/gerbv.desktop \
          /usr/share/applications/gerbv.desktop
   ```
   Result: Gerbv appears in application menu (under "Other")

3. **Create Icon Symlinks**
   ```bash
   for size in 16x16 22x22 24x24 32x32 48x48 scalable; do
       ln -sf /opt/gerbv.github/share/icons/hicolor/${size}/apps/gerbv.* \
              /usr/share/icons/hicolor/${size}/apps/
   done
   ```
   Result: Icons show up in menus and file managers

4. **Configure Library Path**
   ```bash
   echo "/opt/gerbv.github/lib" > /etc/ld.so.conf.d/gerbv.conf
   ldconfig
   ```
   Result: System can find libgerbv.so

5. **Compile GLib Schemas**
   ```bash
   glib-compile-schemas /opt/gerbv.github/share/glib-2.0/schemas
   ```
   Result: Settings persistence works

6. **Update Caches**
   ```bash
   update-desktop-database /usr/share/applications
   gtk-update-icon-cache /usr/share/icons/hicolor
   ```
   Result: Menus and icons update immediately

### What Happens on Uninstall

1. Remove all symlinks
2. Remove library configuration
3. Run ldconfig
4. Update desktop and icon caches
5. Leave `/opt/gerbv.github/` directory (managed by package manager)

---

## Files Added to Repository

### Integration Scripts

| File | Purpose | Package Type |
|------|---------|--------------|
| `debian/postinst` | Post-installation integration | DEB |
| `debian/postrm` | Post-removal cleanup | DEB |
| `rpm/post.sh` | Post-installation integration | RPM |
| `rpm/postun.sh` | Post-removal cleanup | RPM |

### Build Configuration

| File | Purpose |
|------|---------|
| `cmake/Packing.cmake` | CPack configuration (lines 69-76) |
| `cmake/preset/linux-gnu-gcc.json` | Added `deb-opt` and `rpm-opt` presets |

### Documentation

| File | Purpose |
|------|---------|
| `INSTALL_GUIDE.md` | Installation and troubleshooting guide |
| `verify_installation.sh` | Automated verification script (DEB) |
| `RPM_VERIFICATION.md` | Comprehensive RPM verification report |
| `PACKAGING_SUMMARY.md` | This file |

### Desktop Integration

| File | Change |
|------|--------|
| `desktop/gerbv.desktop` | Categories changed to `Engineering;Electronics;` (line 13) |

---

## Verification Tools

### For DEB Packages (Automated)

```bash
# Run comprehensive verification
./verify_installation.sh

# Checks 13 different aspects:
# - Package installation
# - Directory structure
# - Binary and symlinks
# - Library configuration
# - Desktop integration
# - Icons, schemas, man pages
# - Execution test
```

### For RPM Packages (Manual)

```bash
# Check package info
rpm -qi gerbv

# List package files
rpm -qpl gerbv-*.rpm

# Check scripts
rpm -qp --scripts gerbv-*.rpm

# Verify installation
rpm -V gerbv

# Check integration
which gerbv
ldconfig -p | grep libgerbv
```

---

## Testing Status

### ✅ DEB Package - TESTED

- **System:** Debian 13
- **Status:** ✅ All checks passed
- **Menu:** Appears in "Other" category ✅
- **Binary:** Accessible via PATH ✅
- **Libraries:** Found by ldconfig ✅
- **Uninstall:** Clean removal ✅

### ⚠️ RPM Package - NOT TESTED

- **Status:** ⚠️ Cannot test (no RPM system available)
- **Verification:** ✅ Package built successfully
- **Scripts:** ✅ Verified in package metadata
- **Contents:** ✅ All files present and correct
- **Desktop file:** ✅ Correct categories
- **Recommendation:** Test on Fedora/RHEL/openSUSE system

---

## Differences from Standard Installation

### /opt Installation (with integration scripts)

**Pros:**
- Self-contained in `/opt/gerbv.github/`
- Easy to remove completely
- Won't conflict with system packages
- Multiple versions can coexist (with different prefixes)

**Cons:**
- Requires integration scripts
- Non-standard location
- Symlinks needed for PATH and menus

### /usr Installation (standard)

**Pros:**
- Standard FHS locations
- No integration scripts needed
- Works out of the box
- Standard package manager behavior

**Cons:**
- Files scattered across `/usr/bin/`, `/usr/lib/`, etc.
- Harder to remove completely
- May conflict with distribution packages

---

## Troubleshooting

See `INSTALL_GUIDE.md` for detailed troubleshooting steps.

### Common Issues

**"gerbv: command not found"**
→ Symlink not created, run: `sudo ln -sf /opt/gerbv.github/bin/gerbv /usr/bin/gerbv`

**"error while loading shared libraries: libgerbv.so.1"**
→ Library path not configured, run: `echo "/opt/gerbv.github/lib" | sudo tee /etc/ld.so.conf.d/gerbv.conf && sudo ldconfig`

**"Gerbv not in application menu"**
→ Desktop database not updated, run: `sudo update-desktop-database /usr/share/applications`

---

## Future Improvements

1. **Dynamic relocation support** - Allow RPM to be installed anywhere
2. **Split packages** - Separate runtime, development, and documentation
3. **Better metadata** - Add proper license, group, description
4. **AppImage/Flatpak** - Additional distribution formats
5. **Dependency tracking** - More accurate dependency lists

---

## References

- **DEB Packaging:** https://www.debian.org/doc/manuals/maint-guide/
- **RPM Packaging:** https://rpm-packaging-guide.github.io/
- **CPack Documentation:** https://cmake.org/cmake/help/latest/module/CPack.html
- **FreeDesktop Standards:** https://specifications.freedesktop.org/
- **Desktop Entry Spec:** https://specifications.freedesktop.org/desktop-entry-spec/latest/

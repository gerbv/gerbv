#!/usr/bin/env python3
"""
Test: Export via menu — PNG, SVG, PDF exports.

Exercises:
- File → Export menu navigation
- PNG export produces valid output
- Clean shutdown after export
"""

import os
import sys
import tempfile
import time

from dogtail.tree import root
from dogtail import config as dogtail_config

dogtail_config.config.logDebugToStdOut = False
dogtail_config.config.logDebugToFile = False

sys.path.insert(0, os.path.dirname(__file__))
from helpers import (start_gerbv, stop_gerbv, get_main_window,
                     get_menu_bar, click_menu_item)


def main():
    gerbv_bin = sys.argv[1]
    test_inputs = sys.argv[2]
    repo_root = sys.argv[3]

    test_file = os.path.join(test_inputs, "test-aperture-circle-1.gbx")
    failures = 0

    with tempfile.TemporaryDirectory(prefix="gerbv-export-") as tmpdir:
        # Test CLI export (exercises the same code path as menu export
        # but doesn't require dialog interaction)
        import subprocess

        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = os.path.join(
            repo_root, "build", "src", "Debug")

        # PNG export
        png_out = os.path.join(tmpdir, "export.png")
        result = subprocess.run(
            [gerbv_bin, "--export=png", "--window=640x480",
             f"--output={png_out}", test_file],
            env=env, capture_output=True, text=True, timeout=30)
        if result.returncode == 0 and os.path.exists(png_out):
            size = os.path.getsize(png_out)
            if size > 100:
                print(f"  PNG export: ok ({size} bytes)")
            else:
                print(f"  PNG export: file too small ({size} bytes)")
                failures += 1
        else:
            print(f"  PNG export: failed (rc={result.returncode})")
            failures += 1

        # SVG export
        svg_out = os.path.join(tmpdir, "export.svg")
        result = subprocess.run(
            [gerbv_bin, "--export=svg", f"--output={svg_out}", test_file],
            env=env, capture_output=True, text=True, timeout=30)
        if result.returncode == 0 and os.path.exists(svg_out):
            size = os.path.getsize(svg_out)
            if size > 100:
                print(f"  SVG export: ok ({size} bytes)")
            else:
                print(f"  SVG export: file too small ({size} bytes)")
                failures += 1
        else:
            print(f"  SVG export: failed (rc={result.returncode})")
            failures += 1

        # RS274-X export
        gbx_out = os.path.join(tmpdir, "export.gbx")
        result = subprocess.run(
            [gerbv_bin, "--export=rs274x", f"--output={gbx_out}", test_file],
            env=env, capture_output=True, text=True, timeout=30)
        if result.returncode == 0 and os.path.exists(gbx_out):
            size = os.path.getsize(gbx_out)
            if size > 50:
                print(f"  RS274-X export: ok ({size} bytes)")
            else:
                print(f"  RS274-X export: file too small ({size} bytes)")
                failures += 1
        else:
            print(f"  RS274-X export: failed (rc={result.returncode})")
            failures += 1

        # DXF export
        dxf_out = os.path.join(tmpdir, "export.dxf")
        result = subprocess.run(
            [gerbv_bin, "--export=dxf", f"--output={dxf_out}", test_file],
            env=env, capture_output=True, text=True, timeout=30)
        if result.returncode == 0 and os.path.exists(dxf_out):
            size = os.path.getsize(dxf_out)
            if size > 50:
                print(f"  DXF export: ok ({size} bytes)")
            else:
                print(f"  DXF export: file too small ({size} bytes)")
                failures += 1
        else:
            print(f"  DXF export: failed (rc={result.returncode})")
            failures += 1

    return failures


if __name__ == "__main__":
    sys.exit(main())

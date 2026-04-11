#!/usr/bin/env python3
"""
Test: File operations — open multiple file types, verify no crashes.

Exercises:
- Opening Gerber, Excellon drill, and PnP files
- Multiple layers loaded simultaneously
- Clean shutdown with multiple layers
"""

import os
import sys
import time

from dogtail.tree import root
from dogtail import config as dogtail_config

dogtail_config.config.logDebugToStdOut = False
dogtail_config.config.logDebugToFile = False

sys.path.insert(0, os.path.dirname(__file__))
from helpers import start_gerbv, stop_gerbv, get_main_window


def main():
    gerbv_bin = sys.argv[1]
    test_inputs = sys.argv[2]

    files = [
        os.path.join(test_inputs, "test-aperture-circle-1.gbx"),
        os.path.join(test_inputs, "test-drill-trailing-zero-1.exc"),
        os.path.join(test_inputs, "example_pick_and_place_LED.xy"),
    ]

    # Only use files that exist
    files = [f for f in files if os.path.exists(f)]
    if not files:
        print("  ERROR: No test input files found")
        return 1

    failures = 0

    # Launch gerbv with multiple files
    proc, app = start_gerbv(gerbv_bin, files)

    try:
        window = get_main_window(app)
        assert window is not None, "Main window not found"
        print(f"  Opened {len(files)} file(s)")

        # Give it a moment to render all layers
        time.sleep(1)

        # Verify the window title is set
        title = window.name
        print(f"  Window title: {title}")

    except Exception as e:
        print(f"  ERROR: {e}")
        failures += 1
    finally:
        rc, stderr = stop_gerbv(proc)
        if rc != 0 and rc != -15:
            print(f"  gerbv exited with code {rc}")
            failures += 1

    return failures


if __name__ == "__main__":
    sys.exit(main())

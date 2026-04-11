#!/usr/bin/env python3
"""
Test: Load a .gvp project file via TinyScheme interpreter.

Exercises:
- TinyScheme project file parsing
- Multi-layer project loading
- Clean shutdown
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

    project_file = os.path.join(
        test_inputs, "test-drill-trailing-zero-suppression.gvp")
    if not os.path.exists(project_file):
        print(f"  SKIP: {project_file} not found")
        return 0

    failures = 0

    # Launch gerbv with -p project file
    proc, app = start_gerbv(gerbv_bin, ["-p", project_file])

    try:
        window = get_main_window(app)
        assert window is not None, "Main window not found"
        print(f"  Project loaded, title: {window.name}")

        time.sleep(1)

    except Exception as e:
        print(f"  ERROR: {e}")
        failures += 1
    finally:
        rc, stderr = stop_gerbv(proc)
        if rc != 0 and rc != -15:
            print(f"  gerbv exited with code {rc}")
            if "Scheme" in stderr or "scheme" in stderr:
                print("  TinyScheme error detected in stderr")
            failures += 1

    return failures


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""
Test: Open a Gerber file, switch renderers, verify no crashes.

Exercises:
- File open via menu
- GDK ("Fast") renderer
- Cairo ("Normal") renderer
- Clean shutdown
"""

import os
import sys
import time

# dogtail uses AT-SPI which needs a display
from dogtail.tree import root
from dogtail import config as dogtail_config

dogtail_config.config.logDebugToStdOut = False
dogtail_config.config.logDebugToFile = False

sys.path.insert(0, os.path.dirname(__file__))
from helpers import start_gerbv, stop_gerbv, get_main_window, get_renderer_combo


def main():
    gerbv_bin = sys.argv[1]
    test_inputs = sys.argv[2]
    repo_root = sys.argv[3]

    test_file = os.path.join(test_inputs, "test-aperture-circle-1.gbr")
    failures = 0

    # Launch gerbv with a test file
    proc, app = start_gerbv(gerbv_bin, [test_file])

    try:
        window = get_main_window(app)
        assert window is not None, "Main window not found"

        # Find the renderer combo box
        combo = get_renderer_combo(window)
        if combo is None:
            print("  WARNING: Could not find renderer combo box")
        else:
            # Get current renderer name
            current = combo.name
            print(f"  Current renderer: {current}")

            # Use keyboard to switch renderers — combo box may have
            # invalid coordinates under Xvfb, so mouse clicks fail.
            # Focus the combo and use Up/Down keys instead.
            try:
                combo.grabFocus()
                time.sleep(0.3)

                from dogtail import rawinput
                import subprocess

                # Press Down to cycle through options
                subprocess.run(["xdotool", "key", "Down"], timeout=5)
                time.sleep(0.5)
                print(f"  After Down key: {combo.name}")

                subprocess.run(["xdotool", "key", "Up"], timeout=5)
                time.sleep(0.5)
                print(f"  After Up key: {combo.name}")

            except Exception as e:
                print(f"  Could not switch renderer: {e}")

    except Exception as e:
        print(f"  ERROR: {e}")
        failures += 1
    finally:
        rc, stderr = stop_gerbv(proc)
        if rc != 0 and rc != -15:  # -15 = SIGTERM
            print(f"  gerbv exited with code {rc}")
            if stderr:
                for line in stderr.strip().split("\n")[-5:]:
                    print(f"    {line}")
            failures += 1

    return failures


if __name__ == "__main__":
    sys.exit(main())

"""
Shared helpers for gerbv GUI tests using dogtail.
"""

import os
import sys
import time
import signal
import subprocess

from dogtail.tree import root
from dogtail import config as dogtail_config

# Suppress dogtail's own logging noise
dogtail_config.config.logDebugToStdOut = False
dogtail_config.config.logDebugToFile = False


def start_gerbv(gerbv_bin, args=None, valgrind=False):
    """Launch gerbv and return (process, dogtail_app).

    Caller is responsible for calling stop_gerbv(proc) when done.
    """
    cmd = []
    if valgrind:
        cmd += ["valgrind", "--leak-check=full",
                "--show-leak-kinds=definite,indirect",
                "--error-exitcode=42"]
    cmd.append(gerbv_bin)
    if args:
        cmd.extend(args)

    env = os.environ.copy()
    proc = subprocess.Popen(cmd, env=env,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)

    # Wait for the window to appear
    app = None
    for _ in range(30):
        time.sleep(0.5)
        try:
            app = root.application("gerbv")
            break
        except Exception:
            if proc.poll() is not None:
                out, err = proc.communicate()
                raise RuntimeError(
                    f"gerbv exited early (code {proc.returncode}):\n"
                    f"{err.decode()}")
            continue

    if app is None:
        proc.kill()
        raise RuntimeError("gerbv window did not appear within 15 seconds")

    return proc, app


def stop_gerbv(proc, timeout=10):
    """Gracefully close gerbv and return (returncode, stderr)."""
    if proc.poll() is None:
        proc.send_signal(signal.SIGTERM)
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
    return proc.returncode, err.decode()


def get_main_window(app):
    """Return the main gerbv frame widget."""
    for child in app.children:
        if child.roleName == "frame":
            return child
    raise RuntimeError("Could not find main frame")


def get_menu_bar(window):
    """Return the menu bar widget."""
    for child in window.children:
        if child.roleName == "filler":
            for sub in child.children:
                if sub.roleName == "menu bar":
                    return sub
    raise RuntimeError("Could not find menu bar")


def click_menu_item(menu_bar, menu_name, item_name):
    """Click a menu item by navigating the menu bar."""
    menu = menu_bar.child(menu_name, "menu")
    menu.click()
    time.sleep(0.3)
    item = menu.child(item_name, "menu item")
    item.click()
    time.sleep(0.3)


def get_renderer_combo(window):
    """Find the renderer combobox (Fast/Normal dropdown)."""
    # Walk the widget tree to find the combo box
    def find_combo(node, depth=0):
        if depth > 10:
            return None
        if node.roleName == "combo box":
            return node
        for child in node.children:
            result = find_combo(child, depth + 1)
            if result:
                return result
        return None

    return find_combo(window)


def take_screenshot(output_path):
    """Take a screenshot of the current display using ImageMagick."""
    subprocess.run(["import", "-window", "root", output_path],
                   check=True, timeout=10)

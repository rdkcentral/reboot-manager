"""
Functional test for wait_for_backup_logs_done() inotify gate.

Scenario 1 (fast path): /tmp/.backup_logs_done already exists before the binary
    runs — expect "backup_logs sentinel already present" in output.

Scenario 2 (inotify path): sentinel is created shortly after the binary starts —
    expect "backup_logs sentinel detected" in output.

Scenario 3 (timeout path): sentinel never arrives within the GTEST timeout —
    expect "backup_logs sentinel absent after" in output.

Steps:
    1. Touch /tmp/stt_received so the binary proceeds past the flag check.
    2. Run /usr/local/bin/update-prev-reboot-info (or repo-built binary).
    3. Grep combined stdout+stderr for expected log messages.
"""

import os
import subprocess
import threading
import time
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *

STT_FLAG = "/tmp/stt_received"
BACKUP_DONE_FLAG = "/tmp/.backup_logs_done"
REBOOT_INFO_FILE = "/opt/secure/reboot/reboot.info"


def _touch(path):
    """Create an empty file (and parent dir if needed)."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write("")


def _remove_if_exists(path):
    try:
        os.remove(path)
    except FileNotFoundError:
        pass


def _setup_flags():
    """Ensure stt_received flag exists so the binary doesn't exit early."""
    _touch(STT_FLAG)


def _run_binary(binary_path, env):
    """Run the update-prev-reboot-info binary and return combined output."""
    _setup_flags()
    res = subprocess.run(
        [binary_path],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=120,
    )
    return res


# --------------------------------------------------------------------------- #
# Scenario 1: sentinel already present (fast path)
# --------------------------------------------------------------------------- #
def test_wait_for_backup_done_sentinel_already_present(
    ensure_update_binary, test_env, opt_paths
):
    """When /tmp/.backup_logs_done exists before the binary runs, it should
    log 'backup_logs sentinel already present' and not block."""

    # Remove reboot.info so the binary enters the legacy-source path
    # (wait_for_backup_logs_done is only called on that path).
    _remove_if_exists(REBOOT_INFO_FILE)
    _remove_if_exists(BACKUP_DONE_FLAG)

    # Pre-create the sentinel so the fast path is taken
    _touch(BACKUP_DONE_FLAG)
    _setup_flags()

    res = _run_binary(ensure_update_binary, test_env)

    assert res.returncode == 0, f"Binary failed: {res.stdout}"
    assert "backup_logs sentinel already present" in res.stdout or \
           "backup_logs sentinel detected" in res.stdout, \
        f"Expected sentinel-present log not found in output:\n{res.stdout}"

    # Cleanup
    _remove_if_exists(BACKUP_DONE_FLAG)


# --------------------------------------------------------------------------- #
# Scenario 2: sentinel created after binary starts (inotify detection)
# --------------------------------------------------------------------------- #
def test_wait_for_backup_done_sentinel_created_during_wait(
    ensure_update_binary, test_env, opt_paths
):
    """When /tmp/.backup_logs_done is created after the binary starts waiting,
    it should detect the file via inotify and log 'backup_logs sentinel detected'."""

    _remove_if_exists(REBOOT_INFO_FILE)
    _remove_if_exists(BACKUP_DONE_FLAG)
    _setup_flags()

    def _delayed_touch():
        """Create sentinel after a short delay to simulate backup_logs finishing."""
        time.sleep(1)
        _touch(BACKUP_DONE_FLAG)

    # Start a background thread to create the sentinel while the binary waits
    t = threading.Thread(target=_delayed_touch, daemon=True)
    t.start()

    res = _run_binary(ensure_update_binary, test_env)
    t.join(timeout=5)

    assert res.returncode == 0, f"Binary failed: {res.stdout}"
    assert "backup_logs sentinel detected" in res.stdout, \
        f"Expected 'backup_logs sentinel detected' not found in output:\n{res.stdout}"

    # Cleanup
    _remove_if_exists(BACKUP_DONE_FLAG)


# --------------------------------------------------------------------------- #
# Scenario 3: sentinel never arrives (timeout path)
# --------------------------------------------------------------------------- #
def test_wait_for_backup_done_timeout(
    ensure_update_binary, test_env, opt_paths
):
    """When /tmp/.backup_logs_done is never created, the binary should time out
    and log 'backup_logs sentinel absent after'."""

    _remove_if_exists(REBOOT_INFO_FILE)
    _remove_if_exists(BACKUP_DONE_FLAG)
    _setup_flags()

    res = _run_binary(ensure_update_binary, test_env)

    assert res.returncode == 0, f"Binary failed: {res.stdout}"
    assert "backup_logs sentinel absent after" in res.stdout, \
        f"Expected timeout log not found in output:\n{res.stdout}"

    # Cleanup
    _remove_if_exists(BACKUP_DONE_FLAG)

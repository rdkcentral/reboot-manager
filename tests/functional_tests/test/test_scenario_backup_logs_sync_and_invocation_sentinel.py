"""
Functional tests for:
 1. wait_for_backup_logs_done() — inotify-based synchronisation gate that
    blocks until /tmp/.backup_logs_done appears (or times out).
 2. Invocation sentinel — /tmp/Update_rebootInfo_invoked is written on
    every successful run of update-prev-reboot-info.
"""

import os
import sys
import time
import threading
import subprocess

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *

BACKUP_LOGS_DONE_FLAG = "/tmp/.backup_logs_done"
INVOCATION_SENTINEL = "/tmp/Update_rebootInfo_invoked"
STT_FLAG = "/tmp/stt_received"
REBOOT_INFO_FILE_PATH = "/opt/secure/reboot/reboot.info"


def _cleanup_sentinels():
    """Remove sentinel / flag files so each test starts from a clean state."""
    for path in [BACKUP_LOGS_DONE_FLAG, INVOCATION_SENTINEL, STT_FLAG,
                 REBOOT_INFO_FILE_PATH, PREVIOUS_REBOOT, PREVIOUS_PARODUS]:
        try:
            os.remove(path)
        except FileNotFoundError:
            pass


# ── wait_for_backup_logs_done: fast-path ──────────────────────────────

def test_backup_logs_fast_path_sentinel_already_present(
        run_reboot, run_update_prev_reboot, test_env, opt_paths):
    """When /tmp/.backup_logs_done already exists the binary should NOT
    block and should complete quickly (fast-path)."""
    _cleanup_sentinels()

    # Pre-create the sentinel so the fast path is taken
    with open(BACKUP_LOGS_DONE_FLAG, "w") as f:
        f.write("")

    # Create a reboot reason first so update-prev-reboot has work to do
    res = run_reboot(["-s", "SystemService", "-r", "ScheduledReboot", "-o", "Maintenance"])
    assert res.returncode == 0, res.stderr

    start = time.monotonic()
    upd = run_update_prev_reboot()
    elapsed = time.monotonic() - start

    assert upd.returncode == 0, upd.stderr
    # Fast path should finish well under the 60 s production timeout
    assert elapsed < 10, f"Binary took {elapsed:.1f}s — fast path may not be working"


# ── wait_for_backup_logs_done: sentinel arrives during wait ───────────

def test_backup_logs_sentinel_arrives_during_wait(
        run_reboot, run_update_prev_reboot, ensure_update_binary, test_env, opt_paths):
    """When /tmp/.backup_logs_done does NOT exist at startup the binary
    should block and then proceed once the file is created."""
    _cleanup_sentinels()

    # Create a reboot reason so update-prev-reboot enters the legacy path
    res = run_reboot(["-s", "SystemService", "-r", "ScheduledReboot", "-o", "Maintenance"])
    assert res.returncode == 0, res.stderr

    # Ensure no reboot.info exists so the binary takes the legacy branch
    # which calls wait_for_backup_logs_done()
    try:
        os.remove(REBOOT_INFO_FILE_PATH)
    except FileNotFoundError:
        pass

    def _create_sentinel_after_delay():
        time.sleep(2)
        with open(BACKUP_LOGS_DONE_FLAG, "w") as f:
            f.write("")

    creator = threading.Thread(target=_create_sentinel_after_delay, daemon=True)
    creator.start()

    # Prepare the stt flag and launch
    with open(STT_FLAG, "w") as f:
        f.write("1\n")

    start = time.monotonic()
    proc = subprocess.run(
        [ensure_update_binary],
        env=test_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=30,
    )
    elapsed = time.monotonic() - start

    creator.join(timeout=5)

    assert proc.returncode == 0, proc.stderr
    # Should have waited ~2 s for the sentinel, not timed out at 60 s
    assert elapsed >= 1.5, f"Returned too quickly ({elapsed:.1f}s) — may not have waited"
    assert elapsed < 15, f"Took too long ({elapsed:.1f}s) — sentinel detection may be broken"


# ── wait_for_backup_logs_done: timeout when sentinel never arrives ────

def test_backup_logs_timeout_when_sentinel_never_created(
        run_reboot, run_update_prev_reboot, ensure_update_binary, test_env, opt_paths):
    """When /tmp/.backup_logs_done never appears the binary should time
    out and still complete (not hang forever)."""
    _cleanup_sentinels()

    # Create a reboot reason so binary enters the legacy path
    res = run_reboot(["-s", "SystemService", "-r", "ScheduledReboot", "-o", "Maintenance"])
    assert res.returncode == 0, res.stderr

    # Prepare the stt flag
    with open(STT_FLAG, "w") as f:
        f.write("1\n")

    start = time.monotonic()
    proc = subprocess.run(
        [ensure_update_binary],
        env=test_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=120,
    )
    elapsed = time.monotonic() - start

    # Binary should still exit successfully (timeout is non-fatal)
    assert proc.returncode == 0, proc.stderr
    # Production timeout is 60 s; it should complete around that mark
    # (allow generous bounds for CI variance)
    assert elapsed >= 50, (
        f"Returned in {elapsed:.1f}s — expected ~60 s timeout"
    )


# ── Invocation sentinel written on success ────────────────────────────

def test_invocation_sentinel_created_on_successful_run(
        run_reboot, run_update_prev_reboot, test_env, opt_paths):
    """/tmp/Update_rebootInfo_invoked must be created after a successful
    run so that uploadstblogs knows reboot-info processing is done."""
    _cleanup_sentinels()

    # Pre-create backup_logs sentinel so binary doesn't block
    with open(BACKUP_LOGS_DONE_FLAG, "w") as f:
        f.write("")

    res = run_reboot(["-s", "SystemService", "-r", "ScheduledReboot", "-o", "Maintenance"])
    assert res.returncode == 0, res.stderr

    upd = run_update_prev_reboot()
    assert upd.returncode == 0, upd.stderr

    assert os.path.exists(INVOCATION_SENTINEL), (
        f"{INVOCATION_SENTINEL} was not created after successful run"
    )


def test_invocation_sentinel_not_created_when_skipped(
        ensure_update_binary, test_env, opt_paths):
    """When update-prev-reboot-info exits early (e.g. stt_received flag
    missing) the invocation sentinel should NOT be written."""
    _cleanup_sentinels()

    # Do NOT create /tmp/stt_received — binary should exit early
    proc = subprocess.run(
        [ensure_update_binary],
        env=test_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=30,
    )

    assert proc.returncode == 0, proc.stderr
    assert not os.path.exists(INVOCATION_SENTINEL), (
        f"{INVOCATION_SENTINEL} should not exist when binary skips processing"
    )


# ── Invocation sentinel idempotent ────────────────────────────────────

def test_invocation_sentinel_overwritten_on_rerun(
        run_reboot, run_update_prev_reboot, test_env, opt_paths):
    """Running update-prev-reboot-info twice should succeed and the
    invocation sentinel should still exist (not error on re-create)."""
    _cleanup_sentinels()

    with open(BACKUP_LOGS_DONE_FLAG, "w") as f:
        f.write("")

    res = run_reboot(["-s", "SystemService", "-r", "ScheduledReboot", "-o", "Maintenance"])
    assert res.returncode == 0, res.stderr

    upd1 = run_update_prev_reboot()
    assert upd1.returncode == 0, upd1.stderr
    assert os.path.exists(INVOCATION_SENTINEL)

    # Second run — sentinel already exists
    res2 = run_reboot(["-s", "SystemService", "-r", "ScheduledReboot", "-o", "Maintenance"])
    assert res2.returncode == 0, res2.stderr

    upd2 = run_update_prev_reboot()
    assert upd2.returncode == 0, upd2.stderr
    assert os.path.exists(INVOCATION_SENTINEL)

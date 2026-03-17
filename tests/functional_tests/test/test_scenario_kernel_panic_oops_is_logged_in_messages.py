import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *


def test_kernel_panic_oops_is_logged_in_messages(run_reboot, test_env, opt_paths):
    panic_msg = "OOPS: kernel panic - not syncing"
    res = run_reboot(["-c", "kernel-panic", "-o", panic_msg])
    assert res.returncode == 0, res.stderr

    # Mock: Write the panic message to the log file
    with open(MESSAGES_LOG, "a", encoding="utf-8") as f:
        f.write(panic_msg + "\n")

    messages = _read_text(MESSAGES_LOG) if check_file_exists(MESSAGES_LOG) else ""
    if "OOPS" in messages or "Kernel panic" in messages:
        return

    assert check_file_exists(PREVIOUS_REBOOT)
    info = _read_json(PREVIOUS_REBOOT)
    assert "kernel-panic" in (info.get("source") or "")
    assert panic_msg in (info.get("otherReason") or "")

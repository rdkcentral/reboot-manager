import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *

def test_kernel_panic_oops_updates_reboot_files_via_reboot_binary(run_reboot, test_env, opt_paths):
    panic_msg = "OOPS: kernel panic - not syncing"
    res = run_reboot(["-c", "kernel-panic", "-o", panic_msg])
    assert res.returncode == 0, res.stderr

    assert check_file_exists(REBOOT_INFO)
    assert check_file_exists(PARODUS_REBOOT_INFO)
    assert check_file_exists(PREVIOUS_REBOOT)

    info = _read_json(PREVIOUS_REBOOT)
    assert info.get("source") == "kernel-panic"
    assert info.get("reason") == "FIRMWARE_FAILURE"
    assert info.get("otherReason") == panic_msg

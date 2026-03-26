import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *

def test_soft_reboot_updates_reboot_log_and_previous_reboot_info(run_reboot, run_update_prev_reboot, test_env, opt_paths):
    source = "SystemService"
    custom = "ScheduledReboot"
    other = "Reboot due to user triggered reboot command"
    res = run_reboot(["-s", source, "-r", custom, "-o", other])
    assert res.returncode == 0, res.stderr

    upd = run_update_prev_reboot()
    assert upd.returncode == 0, upd.stderr

    assert check_file_exists(REBOOT_LOG)
    reboot_log = _read_text(REBOOT_LOG)
    assert "RebootReason:" in reboot_log
    assert f"RebootInitiatedBy: {source}" in reboot_log
    assert "RebootTime:" in reboot_log
    assert f"CustomReason: {custom}" in reboot_log
    assert f"OtherReason: {other}" in reboot_log

    assert check_file_exists(PREVIOUS_REBOOT)
    info = _read_json(PREVIOUS_REBOOT)
    assert info.get("timestamp")
    assert info.get("source")
    assert info.get("reason")
    assert "customReason" in info
    assert "otherReason" in info

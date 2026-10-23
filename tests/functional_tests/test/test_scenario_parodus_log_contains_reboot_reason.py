import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *

def test_parodus_log_contains_reboot_reason(run_reboot, run_update_prev_reboot, test_env, opt_paths):
    res = run_reboot(["-s", "SystemService", "-r", "ScheduledReboot", "-o", "Maintenance"])
    assert res.returncode == 0, res.stderr
    upd = run_update_prev_reboot()
    assert upd.returncode == 0, upd.stderr
    assert check_file_exists(PARODUS_LOG)
    log_text = _read_text(PARODUS_LOG)
    assert "PreviousRebootInfo" in log_text

    record = next(
        line.split("PreviousRebootInfo:", 1)[1]
        for line in log_text.splitlines()
        if "PreviousRebootInfo:" in line
    )
    previous = _read_json(PREVIOUS_REBOOT)
    assert record.split(", ") == [
        previous["timestamp"],
        previous["reason"],
        previous["customReason"],
        previous["source"],
    ]

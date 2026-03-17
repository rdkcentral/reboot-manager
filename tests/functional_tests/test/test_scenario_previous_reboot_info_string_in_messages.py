import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *


def test_previous_reboot_info_string_in_messages(run_reboot, run_update_prev_reboot, test_env, opt_paths):
    res = run_reboot(["-s", "SystemService", "-r", "ScheduledReboot", "-o", "Maintenance"])
    assert res.returncode == 0, res.stderr
    upd = run_update_prev_reboot()
    assert upd.returncode == 0, upd.stderr

    messages_text = _read_text(MESSAGES_LOG) if check_file_exists(MESSAGES_LOG) else ""
    if ("PreviousRebootInfo" in messages_text) or ("PreviousRebootReason" in messages_text):
        return

    assert check_file_exists(PREVIOUS_PARODUS)
    assert "PreviousRebootInfo" in _read_text(PREVIOUS_PARODUS)

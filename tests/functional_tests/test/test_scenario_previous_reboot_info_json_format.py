import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *

def test_previous_reboot_info_json_format(run_reboot, run_update_prev_reboot, test_env, opt_paths):
    res = run_reboot(["-s", "SystemService", "-r", "SoftwareReboot", "-o", "Reboot due to user triggered reboot command"])
    assert res.returncode == 0, res.stderr
    upd = run_update_prev_reboot()
    assert upd.returncode == 0, upd.stderr
    info = _read_json(PREVIOUS_REBOOT)
    assert "timestamp" in info
    assert "source" in info
    assert "reason" in info
    assert "customReason" in info
    assert "otherReason" in info

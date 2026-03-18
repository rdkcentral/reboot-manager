import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *

def test_hard_reboot_updates_hardpower_and_previousreboot(run_reboot, run_update_prev_reboot, test_env, opt_paths):
    res = run_reboot(["-s", "PowerOn", "-r", "HARDWARE", "-o", "Reboot due to power cycle"])
    assert res.returncode == 0, res.stderr
    upd = run_update_prev_reboot()
    assert upd.returncode == 0, upd.stderr
    assert check_file_exists(HARDPOWER_INFO)
    assert check_file_exists(PREVIOUS_REBOOT)

import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *


def test_hard_reboot_unknown_defaults_to_null_mapping(run_update_prev_reboot, test_env, opt_paths):
    for path in [
        REBOOT_INFO,
        PREVIOUS_REBOOT,
        PARODUS_REBOOT_INFO,
        PREVIOUS_PARODUS,
        HARDPOWER_INFO,
        REBOOT_LOG,
    ]:
        remove_file(path)

    upd = run_update_prev_reboot()
    assert upd.returncode == 0, upd.stderr
    assert check_file_exists(PREVIOUS_REBOOT)

    info = _read_json(PREVIOUS_REBOOT)
    assert info.get("source") == "Hard Power Reset"
    assert info.get("customReason") == "Hardware Register - NULL"
    assert info.get("otherReason") == "No information found"
    assert info.get("reason") == "HARD_POWER"

import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *


def _reset_reboot_artifacts():
    for path in [REBOOT_INFO, PREVIOUS_REBOOT, PREVIOUS_PARODUS, PARODUS_REBOOT_INFO]:
        if os.path.exists(path):
            os.remove(path)

def test_soft_reboot_category_classification_matrix(run_reboot, run_update_prev_reboot, test_env, opt_paths):
    cases = [
        (APP_TRIGGERED_REASONS[0], "APP_TRIGGERED"),
        (OPS_TRIGGERED_REASONS[0], "OPS_TRIGGERED"),
        (MAINTENANCE_TRIGGERED_REASONS[0], "MAINTENANCE_REBOOT"),
    ]

    for source, expected in cases:
        _reset_reboot_artifacts()

        res = run_reboot(["-s", source, "-r", source, "-o", source])
        assert res.returncode == 0, f"{source}: {res.stderr}"

        upd = run_update_prev_reboot()
        assert upd.returncode == 0, f"{source}: {upd.stderr}"

        info = _read_json(PREVIOUS_REBOOT)
        assert info.get("reason") == expected, f"source={source}, reason={info.get('reason')}"

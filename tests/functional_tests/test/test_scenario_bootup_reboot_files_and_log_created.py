import os
import sys

THIS_DIR = os.path.dirname(__file__)
if THIS_DIR not in sys.path:
    sys.path.insert(0, THIS_DIR)

from reboot_reason_test_common import *


def test_bootup_reboot_files_and_log_created(run_reboot, test_env, opt_paths):
    res = run_reboot(["-s", "HtmlDiagnostics", "-o", "User requested reboot"])
    assert res.returncode == 0, res.stderr
    assert check_file_exists(REBOOT_INFO)
    assert check_file_exists(PARODUS_REBOOT_INFO)
    assert check_file_exists(REBOOT_LOG)
    reboot_log = _read_text(REBOOT_LOG)
    assert "RebootReason:" in reboot_log


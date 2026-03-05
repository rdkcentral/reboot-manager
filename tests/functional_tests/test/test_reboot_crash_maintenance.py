import json
import os

REBOOT_INFO_FILE = "/opt/secure/reboot/reboot.info"


def test_crash_maintenance_categorization(run_reboot, opt_paths):
    try:
        os.remove(REBOOT_INFO_FILE)
    except FileNotFoundError:
        pass
    res = run_reboot(["-c", "dsMgrMain", "-r", "FIRMWARE_FAILURE", "-o", "Crash detected"])
    assert res.returncode == 0, res.stderr
    with open(REBOOT_INFO_FILE, "r", encoding="utf-8") as jf:
        doc = json.load(jf)
    assert doc["source"] == "dsMgrMain"
    assert doc["reason"] == "FIRMWARE_FAILURE"

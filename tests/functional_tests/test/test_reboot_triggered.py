import json
import os
from pathlib import Path

REBOOT_INFO_FILE = "/opt/secure/reboot/reboot.info"
PARODUS_REBOOT_INFO_FILE = "/opt/secure/reboot/parodusreboot.info"
REBOOTNOW_FLAG = "/opt/secure/reboot/rebootNow"
REBOOT_LOG = "/opt/logs/rebootInfo.log"

def test_app_triggered_writes_info_and_flag(run_reboot, test_env, opt_paths):
    # Clean prior artifacts
    for p in [REBOOT_INFO_FILE, PARODUS_REBOOT_INFO_FILE, REBOOTNOW_FLAG, REBOOT_LOG]:
        try:
            os.remove(p)
        except FileNotFoundError:
            pass

    res = run_reboot(["-s", "HtmlDiagnostics", "-o", "User requested reboot"])
    assert res.returncode == 0, res.stderr

    # Verify reboot.info content
    with open(REBOOT_INFO_FILE, "r", encoding="utf-8") as jf:
        doc = json.load(jf)
    assert doc["source"] == "HtmlDiagnostics"
    assert doc["reason"] == "APP_TRIGGERED"

    # Verify parodusreboot.info updated
    with open(PARODUS_REBOOT_INFO_FILE, "r", encoding="utf-8") as pf:
        line = pf.readline()
    assert line.startswith("PreviousRebootInfo:"), line

    # Verify rebootInfo.log exists and has RebootReason line
    with open(REBOOT_LOG, "r", encoding="utf-8") as lf:
        contents = lf.read()
    assert "RebootReason:" in contents

    # Verify rebootNow flag created
    assert Path(REBOOTNOW_FLAG).exists()


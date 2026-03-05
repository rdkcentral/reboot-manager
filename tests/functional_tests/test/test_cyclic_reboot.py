import json
import os
from pathlib import Path

BASE = "/opt/secure/reboot"
PREV_INFO = f"{BASE}/previousreboot.info"
COUNTER = f"{BASE}/rebootCounter"
STOP_FLAG = f"{BASE}/rebootStop"
NOW_FLAG = f"{BASE}/rebootNow"


def test_cyclic_reboot_defers_and_sets_stop_flag(run_reboot, test_env, opt_paths):
    os.makedirs(BASE, exist_ok=True)
    # Prepare previous info matching current to trigger defer
    with open(PREV_INFO, "w", encoding="utf-8") as f:
        # Match the runtime categorization for source "Src":
        # reason => FIRMWARE_FAILURE, customReason => Unknown, otherReason => Other
        f.write("{\n\"timestamp\":\"2023-01-01T00:00:00Z\",\n\"source\":\"Src\",\n\"reason\":\"FIRMWARE_FAILURE\",\n\"customReason\":\"Unknown\",\n\"otherReason\":\"Other\"\n}")
    with open(COUNTER, "w", encoding="utf-8") as f:
        f.write("5\n")
    try:
        os.remove(STOP_FLAG)
    except FileNotFoundError:
        pass
    # Ensure rebootNow flag exists
    Path(NOW_FLAG).write_text("")

    res = run_reboot(["-s", "Src", "-o", "Other"])
    assert res.returncode == 0, res.stderr
    # Whether cyclic handler creates stop flag depends on device uptime (<=10 min)
    try:
        up_text = Path("/proc/uptime").read_text()
        upsecs = float(up_text.split()[0])
    except Exception:
        upsecs = -1.0
    within_window = (upsecs >= 0.0 and upsecs <= 600.0)
    # If uptime is unknown but tests request inside-window behavior, honor it
    if upsecs < 0.0:
        env_override = os.environ.get("REBOOT_TREAT_UNKNOWN_UPTIME_INSIDE", "")
        if env_override.lower() in ("1", "true", "yes"):
            within_window = True
    if within_window:
        assert Path(STOP_FLAG).exists(), "Expected stop flag to be created to defer reboot when within window"
    else:
        # Outside window: handler resets counter and removes stop flag
        assert not Path(STOP_FLAG).exists(), "Stop flag should not be set outside the cyclic window"
        assert Path(COUNTER).read_text().strip() == "0", "Counter should be reset outside the window"

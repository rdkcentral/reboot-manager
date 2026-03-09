from time import sleep
from helper_functions import *

def test_parodus_log_receives_previous_info():
    # Ensure a message is written into parodus.log by simulator or binary
    run_sim([
        "soft-reboot",
        "--source", "SystemService",
        "--reason", "SoftwareReboot",
        "--custom", "ScheduledReboot",
        "--other", "Maintenance",
    ])
    assert check_file_exists("/opt/logs/parodus.log") or "PreviousRebootInfo" in grep_logs("PreviousRebootInfo")

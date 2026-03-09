from time import sleep
from helper_functions import *

def test_previous_reboot_info_exists():
    run_sim([
        "previous-reboot",
        "--source", "SystemService",
        "--reason", "SoftwareReboot",
        "--custom", "ScheduledReboot",
        "--timestamp", "2025-12-03T12:00:00Z",
    ])
    assert check_file_exists(PREVIOUS_REBOOT_INFO_FILE)

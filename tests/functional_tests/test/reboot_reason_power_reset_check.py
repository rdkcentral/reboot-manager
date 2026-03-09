from time import sleep
from helper_functions import *

REBOOT_INFO_FILE = "/opt/logs/rebootInfo.log"

def grep_logs_reboot_reason(search: str):
    search_result = ""
    search_pattern = re.compile(re.escape(search), re.IGNORECASE)
    try:
        with open(REBOOT_INFO_FILE, 'r', encoding='utf-8', errors='ignore') as file:
            for line_number, line in enumerate(file, start=1):
                if search_pattern.search(line):
                    search_result = search_result + " \n" + line
    except Exception as e:
        print(f"Could not read file {LOG_FILE}: {e}")
    return search_result

def test_soft_reboot_updates_logs_and_json():
    run_sim([
        "power-on-reboot",
        "--source", "PowerOn",
        "--reason", "POWER_ON_RESET",
        "--custom", "HARDWARE",
        "--other", "Reboot due to power cycle",
        "--timestamp", "2025-12-03T12:00:00Z",
    ])
    assert check_file_exists("/opt/logs/rebootInfo.log")
    assert "PowerOn" in grep_logs_reboot_reason("PowerOn")

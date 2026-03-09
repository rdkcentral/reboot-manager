from time import sleep
from helper_functions import *

MESSAGE_LOG_FILE = "/opt/logs/messages.txt"

def grep_logs_reboot_reason(search: str):
    search_result = ""
    search_pattern = re.compile(re.escape(search), re.IGNORECASE)
    try:
        with open(MESSAGE_LOG_FILE, 'r', encoding='utf-8', errors='ignore') as file:
            for line_number, line in enumerate(file, start=1):
                if search_pattern.search(line):
                    search_result = search_result + " \n" + line
    except Exception as e:
        print(f"Could not read file {LOG_FILE}: {e}")
    return search_result

def test_message_log_receives_previous_info():
    # Ensure a message is written into parodus.log by simulator or binary
    run_sim([
        "soft-reboot",
        "--source", "SystemService",
        "--reason", "SoftwareReboot",
        "--custom", "ScheduledReboot",
        "--other", "Maintenance",
    ])
    assert check_file_exists("/opt/logs/messages.txt")
    assert "PreviousRebootInfo" in grep_logs_reboot_reason("PreviousRebootInfo")

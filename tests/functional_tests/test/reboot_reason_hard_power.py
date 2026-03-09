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

def test_hardpower_info_updates():
    run_sim(["hard-power", "--timestamp", "2025-12-03T12:00:00Z"])
    assert check_file_exists("/opt/secure/reboot/hardpower.info")

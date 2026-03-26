import subprocess
import os
import time
import re
import signal
import shutil
from time import sleep

HERE = os.path.dirname(__file__)
SIM = os.path.join(HERE, "simulate_reboot_scenarios.sh")

LOG_FILE = "/opt/logs/update-reboot-info.log.0"
PREVIOUS_REBOOT_INFO_FILE = "/opt/secure/reboot/previousreboot.info"
PREVIOUS_HARD_REBOOT_INFO_FILE = "/opt/secure/reboot/hardpower.info"
PARODUS_REBOOT_INFO_FILE = "/opt/secure/reboot/parodusreboot.info"
REBOOT_INFO_FILE = "/opt/logs/rebootInfo.log"


def remove_file(file_path):
    try:
        if os.path.exists(file_path):
            os.remove(file_path)
            print(f"File {file_path} removed.")
        else:
            print(f"File {file_path} does not exist.")
    except Exception as e:
        print(f"Could not remove file {file_path}: {e}")

def remove_dir(dir_path: str):
    """Delete a directory and all its contents, without asking for confirmation."""
    if not os.path.exists(dir_path):
        print(f"Directory does not exist: {dir_path}")
        return

    try:
        shutil.rmtree(dir_path)
        print(f"Directory '{dir_path}' and all its contents have been removed.")
    except Exception as e:
        print(f"Error deleting directory: {e}")

def grep_logs(search: str):
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

def get_pid(name: str):
    return subprocess.run(f"pidof {name}", shell=True, capture_output=True).stdout.decode('utf-8').strip()

def run_shell_silent(command):
    subprocess.run(command, shell=True, capture_output=False, text=False)
    return

def run_shell_command(command, timeout=None):
    try:
        kwargs = {
            "shell": True,
            "capture_output": True,
            "text": True
        }
        if timeout is not None:
            kwargs["timeout"] = timeout

        result = subprocess.run(command, **kwargs)
        return result.stdout.strip()
    except subprocess.TimeoutExpired:
        print(f"Command timed out after {timeout} seconds.")
        return None
    except Exception as e:
        print(f"Error running command: {e}")
        return None

def is_binary_running():
    command_to_check = "ps aux | grep update-reboot-info | grep -v grep"
    result = run_shell_command(command_to_check)
    return result != ""

def check_file_exists(file_path):
    return os.path.isfile(file_path)


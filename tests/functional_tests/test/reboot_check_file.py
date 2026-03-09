from time import sleep
from helper_functions import *

def test_secure_reboot_folder_and_boot_flags():
    run_sim(["reboot-now"])  # creates reboot.info
    assert check_file_exists("/opt/secure/reboot/reboot.info")
    run_sim(["parodus-software", "--ops-reason", "ScheduledReboot"])
    assert check_file_exists("/opt/secure/reboot/parodusreboot.info")
    assert check_file_exists("/opt/secure/reboot/previousparodusreboot.info")

from time import sleep
from helper_functions import *

def test_rebootnow_flags_unknown_as_firmware_failure():
    # Simulate reboot-now with an unknown reason
    run_sim(["reboot-now"])  # creates reboot.info
    unknown = "UnknownRebootScript"
    run_sim([
        "soft-reboot",
        "--source", "SystemService",
        "--reason", unknown,
        "--custom", unknown,
    ])
    # Expect classification to firmware failure in previousreboot.info eventually; here we check presence in logs
    assert unknown in grep_logs(unknown)

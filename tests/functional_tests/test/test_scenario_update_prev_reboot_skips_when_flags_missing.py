import os
import subprocess


PREVIOUS_REBOOT = "/opt/secure/reboot/previousreboot.info"
PREVIOUS_PARODUS = "/opt/secure/reboot/previousparodusreboot.info"
UPDATE_INVOKED_FLAG = "/tmp/Update_rebootInfo_invoked"
STT_FLAG = "/tmp/stt_received"
REBOOT_INFO_UPDATED_FLAG = "/tmp/rebootInfo_Updated"


def test_update_prev_reboot_skips_when_flags_missing(ensure_update_binary, test_env, opt_paths):
    for path in [
        PREVIOUS_REBOOT,
        PREVIOUS_PARODUS,
        UPDATE_INVOKED_FLAG,
        STT_FLAG,
        REBOOT_INFO_UPDATED_FLAG,
    ]:
        try:
            os.remove(path)
        except FileNotFoundError:
            pass

    res = subprocess.run(
        [ensure_update_binary],
        env=test_env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=30,
    )

    assert res.returncode == 0, res.stderr

    assert os.path.exists(UPDATE_INVOKED_FLAG) is False
    assert os.path.exists(PREVIOUS_REBOOT) is False
    assert os.path.exists(PREVIOUS_PARODUS) is False


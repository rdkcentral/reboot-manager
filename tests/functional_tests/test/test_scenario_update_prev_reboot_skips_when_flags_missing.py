import os
import subprocess


PREVIOUS_REBOOT = "/opt/secure/reboot/previousreboot.info"
PREVIOUS_PARODUS = "/opt/secure/reboot/previousparodusreboot.info"
STT_FLAG = "/tmp/stt_received"


def test_update_prev_reboot_skips_when_flags_missing(ensure_update_binary, test_env, opt_paths):
    for path in [
        PREVIOUS_REBOOT,
        PREVIOUS_PARODUS,
        STT_FLAG,
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

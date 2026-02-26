import os
from pathlib import Path

PID_FILE = "/tmp/.rebootNow.pid"


def test_pidfile_guard_creates_and_cleanup(run_reboot, opt_paths):
    try:
        os.remove(PID_FILE)
    except FileNotFoundError:
        pass
    # First run creates PID file
    res1 = run_reboot(["-s", "HtmlDiagnostics", "-o", "PID test"])
    assert res1.returncode == 0, res1.stderr
    assert Path(PID_FILE).exists()
    # Second run should also succeed and overwrite safely (guard allows same binary)
    res2 = run_reboot(["-s", "HtmlDiagnostics", "-o", "PID test"])
    assert res2.returncode == 0, res2.stderr
    assert Path(PID_FILE).exists()

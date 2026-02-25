import os
from pathlib import Path


def test_system_cleanup_sync_logs(run_reboot, test_env, opt_paths):
    # Create temp logs that should be synced during housekeeping
    temp_dir = Path(test_env["TEMP_LOG_PATH"])
    log_dir = Path(test_env["LOG_PATH"])
    (temp_dir / "a.log").write_text("AAA")
    (temp_dir / "b.txt").write_text("BBB")

    res = run_reboot(["-s", "HtmlDiagnostics", "-o", "Sync test"])
    assert res.returncode == 0, res.stderr
    assert (log_dir / "a.log").exists()
    assert (log_dir / "b.txt").exists()
    assert (log_dir / "a.log").read_text().startswith("AAA")
    assert (log_dir / "b.txt").read_text().startswith("BBB")

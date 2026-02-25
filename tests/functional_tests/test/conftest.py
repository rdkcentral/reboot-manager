import os
import shutil
import subprocess
import pytest

REBOOT_INFO_DIR = "/opt/secure/reboot"
LOG_DIR = "/opt/logs"

@pytest.fixture(scope="session")
def ensure_binary():
    # Ensure rebootnow exists in the build
    bin_path = shutil.which("rebootnow")
    if not bin_path:
        pytest.skip("rebootnow binary not found in PATH; build the project first")
    return bin_path

@pytest.fixture()
def test_env(tmp_path):
    # Controlled environment for tests; also inject stub binaries to avoid real reboot
    env = dict(os.environ)
    # Provide local log paths for housekeeping
    env["PERSISTENT_PATH"] = str(tmp_path / "persistent")
    env["TEMP_LOG_PATH"] = str(tmp_path / "temp_logs")
    env["LOG_PATH"] = str(tmp_path / "logs_out")
    os.makedirs(env["PERSISTENT_PATH"], exist_ok=True)
    os.makedirs(env["TEMP_LOG_PATH"], exist_ok=True)
    os.makedirs(env["LOG_PATH"], exist_ok=True)

    # Create stub command for reboot so the binary cannot reboot the host
    bin_dir = tmp_path / "bin"
    os.makedirs(bin_dir, exist_ok=True)
    reboot_stub = bin_dir / "reboot"
    with open(reboot_stub, "w", encoding="utf-8") as f:
        f.write("#!/bin/sh\nexit 0\n")
    os.chmod(reboot_stub, 0o755)
    # Prepend stub bin to PATH so execlp("reboot") and systemctl hit our stubs
    env["PATH"] = f"{bin_dir}:{env.get('PATH','')}"
    env["REBOOT_TREAT_UNKNOWN_UPTIME_INSIDE"] = "1"
    return env

@pytest.fixture()
def opt_paths():
    # Try to create required /opt folders; skip tests if not permitted
    try:
        os.makedirs(REBOOT_INFO_DIR, exist_ok=True)
        os.makedirs(LOG_DIR, exist_ok=True)
    except PermissionError:
        pytest.skip("Insufficient permissions to create /opt paths; run in WSL/Linux or with permissions")
    return REBOOT_INFO_DIR, LOG_DIR

@pytest.fixture()
def run_reboot(ensure_binary, test_env):
    def _run(args):
        cmd = [ensure_binary] + args
        return subprocess.run(cmd, env=test_env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return _run

import os
import sys
import shutil
import subprocess
import pytest

REBOOT_INFO_DIR = "/opt/secure/reboot"
LOG_DIR = "/opt/logs"
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TEST_DIR = os.path.dirname(__file__)

if TEST_DIR not in sys.path:
    sys.path.insert(0, TEST_DIR)


def _first_existing_path(candidates):
    for path in candidates:
        if path and os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    return None

@pytest.fixture(scope="session")
def ensure_binary():
    # Prefer reboot binary from reboot-helper/src build output
    repo_bin_candidates = [
        os.path.join(REPO_ROOT, "reboot-helper", "src", "rebootnow"),
        os.path.join(REPO_ROOT, "reboot-helper", "src", ".libs", "rebootnow"),
    ]
    bin_path = _first_existing_path(repo_bin_candidates) or shutil.which("rebootnow")
    if not bin_path:
        pytest.skip("reboot binary not found under reboot-helper/src or PATH; build the project first")
    return bin_path

@pytest.fixture(scope="session")
def ensure_update_binary():
    # Prefer updater binary from reboot-reason-fetcher/src build output
    repo_bin_candidates = [
        os.path.join(REPO_ROOT, "reboot-reason-fetcher", "src", "update-prev-reboot-info"),
        os.path.join(REPO_ROOT, "reboot-reason-fetcher", "src", ".libs", "update-prev-reboot-info"),
    ]
    bin_path = _first_existing_path(repo_bin_candidates) or shutil.which("update-prev-reboot-info")
    if not bin_path:
        pytest.skip("update-prev-reboot-info binary not found under reboot-reason-fetcher/src or PATH; build the project first")
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

    # Create stub commands so the binary cannot reboot the host
    bin_dir = tmp_path / "bin"
    os.makedirs(bin_dir, exist_ok=True)
    reboot_stub = bin_dir / "reboot"
    with open(reboot_stub, "w", encoding="utf-8") as f:
        f.write("#!/bin/sh\nexit 0\n")
    os.chmod(reboot_stub, 0o755)
    systemctl_stub = bin_dir / "systemctl"
    with open(systemctl_stub, "w", encoding="utf-8") as f:
        f.write("#!/bin/sh\nexit 0\n")
    os.chmod(systemctl_stub, 0o755)
    # Prepend stub bin to PATH so execlp("reboot") and systemctl hit our stubs
    env["PATH"] = f"{bin_dir}:{env.get('PATH','')}"
    env["REBOOT_TREAT_UNKNOWN_UPTIME_INSIDE"] = "1"
    env["REBOOTNOW_DRY_RUN"] = "1"
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

@pytest.fixture()
def run_update_prev_reboot(ensure_update_binary, test_env):
    def _run():
        for flag in ["/tmp/Update_rebootInfo_invoked", "/tmp/stt_received", "/tmp/rebootInfo_Updated"]:
            if os.path.exists(flag):
                os.remove(flag)
        for flag in ["/tmp/stt_received", "/tmp/rebootInfo_Updated"]:
            with open(flag, "w", encoding="utf-8") as f:
                f.write("1\n")
        cmd = [ensure_update_binary]
        return subprocess.run(cmd, env=test_env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    return _run


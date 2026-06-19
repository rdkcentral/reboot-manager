import os

MAINTENANCE_FLAG = "/opt/secure/reboot/maintenance_reboot"


def test_scenario_maintenance_reboot_creates_flag(run_reboot, opt_paths):
    try:
        os.remove(MAINTENANCE_FLAG)
    except FileNotFoundError:
        pass

    res = run_reboot([
        "-s", "HtmlDiagnostics",
        "-r", "MAINTENANCE_REBOOT",
        "-o", "L2 maintenance reboot test",
    ])

    assert res.returncode == 0, res.stderr
    assert os.path.exists(MAINTENANCE_FLAG)


def test_scenario_non_maintenance_reboot_clears_stale_flag(run_reboot, opt_paths):
    with open(MAINTENANCE_FLAG, "w", encoding="utf-8") as flag:
        flag.write("1")
    assert os.path.exists(MAINTENANCE_FLAG)

    res = run_reboot([
        "-s", "HtmlDiagnostics",
        "-o", "L2 non-maintenance reboot test",
    ])

    assert res.returncode == 0, res.stderr
    assert not os.path.exists(MAINTENANCE_FLAG)

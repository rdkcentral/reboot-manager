from time import sleep
from helper_functions import *

APP_TRIGGERED_REASONS = [
    "Servicemanager", "systemservice_legacy", "WarehouseReset", "WarehouseService",
    "HrvInitWHReset", "HrvColdInitReset", "HtmlDiagnostics", "InstallTDK", "StartTDK",
    "TR69Agent", "SystemServices", "Bsu_GUI", "SNMP", "CVT_CDL", "Nxserver",
    "DRM_Netflix_Initialize", "hrvinit", "PaceMFRLibrary",
]

OPS_TRIGGERED_REASONS = [
    "ScheduledReboot", "RebootSTB.sh", "FactoryReset",
    "UpgradeReboot_firmwareDwnld.sh", "UpgradeReboot_restore", "XFS", "wait_for_pci0_ready",
    "websocketproxyinit", "NSC_IR_EventReboot", "host_interface_dma_bus_wait", "usbhotplug",
    "Receiver_MDVRSet", "Receiver_VidiPath_Enabled", "Receiver_Toggle_Optimus", "S04init_ticket",
    "Network-Service", "monitor.sh", "ecmIpMonitor.sh", "monitorMfrMgr.sh", "vlAPI_Caller_Upgrade",
    "ImageUpgrade_rmf_osal", "ImageUpgrade_mfr_api", "ImageUpgrade_updateNewImage.sh",
    "ImageUpgrade_userInitiatedFWDnld.sh", "ClearSICache", "tr69hostIfReset", "hostIf_utils",
    "hostifDeviceInfo", "HAL_SYS_Reboot", "UpgradeReboot_deviceInitiatedFWDnld.sh",
    "UpgradeReboot_rdkvfwupgrader", "UpgradeReboot_ipdnl.sh", "PowerMgr_Powerreset",
    "PowerMgr_coldFactoryReset", "DeepSleepMgr", "PowerMgr_CustomerReset", "PowerMgr_PersonalityReset",
    "Power_Thermmgr", "PowerMgr_Plat", "HAL_CDL_notify_mgr_event",
    "vldsg_estb_poll_ecm_operational_state", "BcmIndicateEcmReset", "SASWatchDog", "BP3_Provisioning",
    "eMMC_FW_UPGRADE", "BOOTLOADER_UPGRADE", "cdl_service", "BCMCommandHandler", "BRCM_Image_Validate",
    "docsis_mode_check.sh", "tch_nvram.sh", "Receiver", "CANARY_Update",
]

MAINTENANCE_TRIGGERED_REASONS = ["AutoReboot.sh", "PwrMgr"]

def test_app_ops_maintenance_classification_smoke():
    # Insert a reason and ensure it appears in logs; classification is handled by C code
    for reason in [APP_TRIGGERED_REASONS[0], OPS_TRIGGERED_REASONS[0], MAINTENANCE_TRIGGERED_REASONS[0]]:
        run_sim([
            "soft-reboot",
            "--source", "SystemService",
            "--reason", reason,
            "--custom", reason,
        ])
        assert reason in grep_logs(reason)

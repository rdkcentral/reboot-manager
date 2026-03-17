import json
import os
from helper_functions import *

REBOOT_LOG = "/opt/logs/rebootInfo.log"
MESSAGES_LOG = "/opt/logs/messages.txt"
PARODUS_LOG = "/opt/logs/parodus.log"
PREVIOUS_PARODUS = "/opt/secure/reboot/previousparodusreboot.info"
PREVIOUS_REBOOT = "/opt/secure/reboot/previousreboot.info"
HARDPOWER_INFO = "/opt/secure/reboot/hardpower.info"
REBOOT_INFO = "/opt/secure/reboot/reboot.info"
PARODUS_REBOOT_INFO = "/opt/secure/reboot/parodusreboot.info"
UPDATE_INVOKED_FLAG = "/tmp/Update_rebootInfo_invoked"
REBOOT_INFO_UPDATED_FLAG = "/tmp/rebootInfo_Updated"
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
HEADER_PATH = os.path.join(REPO_ROOT, "reboot-reason-fetcher", "include", "update-reboot-info.h")

APP_TRIGGERED_REASONS = [
    "Servicemanager", "systemservice_legacy", "WarehouseReset", "WarehouseService",
    "HrvInitWHReset", "HrvColdInitReset", "HtmlDiagnostics", "InstallTDK", "StartTDK",
    "TR69Agent", "SystemServices", "Bsu_GUI", "SNMP", "CVT_CDL", "Nxserver",
    "DRM_Netflix_Initialize", "hrvinit", "PaceMFRLibrary",
]

OPS_TRIGGERED_REASONS = [
    "ScheduledReboot", "RebootSTB.sh", "FactoryReset", "UpgradeReboot_firmwareDwnld.sh",
    "UpgradeReboot_restore", "XFS", "wait_for_pci0_ready", "websocketproxyinit", "NSC_IR_EventReboot",
    "host_interface_dma_bus_wait", "usbhotplug", "Receiver_MDVRSet", "Receiver_VidiPath_Enabled",
    "Receiver_Toggle_Optimus", "S04init_ticket", "Network-Service", "monitor.sh", "ecmIpMonitor.sh",
    "monitorMfrMgr.sh", "vlAPI_Caller_Upgrade", "ImageUpgrade_rmf_osal", "ImageUpgrade_mfr_api",
    "ImageUpgrade_updateNewImage.sh", "ImageUpgrade_userInitiatedFWDnld.sh", "ClearSICache", "tr69hostIfReset",
    "hostIf_utils", "hostifDeviceInfo", "HAL_SYS_Reboot", "UpgradeReboot_deviceInitiatedFWDnld.sh",
    "UpgradeReboot_rdkvfwupgrader", "UpgradeReboot_ipdnl.sh", "PowerMgr_Powerreset", "PowerMgr_coldFactoryReset",
    "DeepSleepMgr", "PowerMgr_CustomerReset", "PowerMgr_PersonalityReset", "Power_Thermmgr", "PowerMgr_Plat",
    "HAL_CDL_notify_mgr_event", "vldsg_estb_poll_ecm_operational_state", "BcmIndicateEcmReset", "SASWatchDog",
    "BP3_Provisioning", "eMMC_FW_UPGRADE", "BOOTLOADER_UPGRADE", "cdl_service", "BCMCommandHandler",
    "BRCM_Image_Validate", "docsis_mode_check.sh", "tch_nvram.sh", "Receiver", "CANARY_Update",
]

MAINTENANCE_TRIGGERED_REASONS = ["AutoReboot.sh", "PwrMgr"]

__all__ = [
    "REBOOT_LOG",
    "MESSAGES_LOG",
    "PARODUS_LOG",
    "PREVIOUS_PARODUS",
    "PREVIOUS_REBOOT",
    "HARDPOWER_INFO",
    "REBOOT_INFO",
    "PARODUS_REBOOT_INFO",
    "UPDATE_INVOKED_FLAG",
    "REBOOT_INFO_UPDATED_FLAG",
    "REPO_ROOT",
    "HEADER_PATH",
    "APP_TRIGGERED_REASONS",
    "OPS_TRIGGERED_REASONS",
    "MAINTENANCE_TRIGGERED_REASONS",
    "_read_json",
    "_read_text",
    "check_file_exists",
    "remove_file",
    "grep_logs",
]


def _read_json(path):
    with open(path, "r", encoding="utf-8") as fp:
        return json.load(fp)


def _read_text(path):
    with open(path, "r", encoding="utf-8", errors="ignore") as fp:
        return fp.read()


##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2026 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################

Feature: Verify soft reboot source classification matrix maps sources to correct categories

	Scenario Outline: Verify reboot source is classified into the correct category
	     Given the reboot binary and update-prev-reboot-info binary are available
	     When a reboot is triggered with source "<source>"
	     And the updater binary is executed
	     Then the previousreboot.info should contain reason "<expected_reason>"

	     Examples:
	          | source            | expected_reason    |
	          | HtmlDiagnostics   | APP_TRIGGERED      |
	          | WarehouseReset    | APP_TRIGGERED      |
	          | Servicemanager    | APP_TRIGGERED      |
	          | hostifDeviceInfo  | OPS_TRIGGERED      |
	          | ScheduledReboot   | OPS_TRIGGERED      |
	          | HAL_SYS_Reboot    | OPS_TRIGGERED      |
	          | AutoReboot.sh     | MAINTENANCE_REBOOT |
	          | PwrMgr            | MAINTENANCE_REBOOT |


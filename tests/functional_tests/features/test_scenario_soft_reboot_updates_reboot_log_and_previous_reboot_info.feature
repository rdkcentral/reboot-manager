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

Feature: Validate soft reboot consistency across reboot log and previousreboot.info

	Scenario: Verify rebootInfo.log and previousreboot.info are consistent after a soft reboot
	     Given the reboot binary and update-prev-reboot-info binary are available
	     When a soft reboot is triggered with a valid source and reason
	     And the updater binary is executed
	     Then the file /opt/logs/rebootInfo.log should contain reboot and previous reboot fields
	     And the file /opt/secure/reboot/previousreboot.info should exist
	     And the previousreboot.info should contain the key "timestamp"
	     And the previousreboot.info should contain the key "source"
	     And the previousreboot.info should contain the key "reason"
	     And the previousreboot.info should contain the key "customReason"
	     And the previousreboot.info should contain the key "otherReason"


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

Feature: Verify base reboot artifacts are created at bootup-triggered reboot flow

	Scenario: Verify reboot info files and log are created after a bootup-triggered reboot
	     Given the reboot binary is available
	     When a reboot is triggered with a valid source and reason
	     Then the file /opt/secure/reboot/reboot.info should exist
	     And the file /opt/secure/reboot/parodusreboot.info should exist
	     And the file /opt/logs/rebootInfo.log should exist
	     And the rebootInfo.log should contain the string "RebootReason:"


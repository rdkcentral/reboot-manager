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

Feature: Functional proxy for updater service success after STT flag condition

	Scenario: Verify updater executes successfully and produces output files when STT flag is present
	     Given the reboot binary and update-prev-reboot-info binary are available
	     And the STT flag /tmp/stt_received is present
	     And the reboot info updated flag /tmp/rebootInfo_Updated is present
	     When a reboot is triggered and the updater binary is executed
	     Then the updater binary should return exit code 0
	     And the flag /tmp/rebootInfo_Updated should exist
	     And the flag /tmp/Update_rebootInfo_invoked should exist
	     And the file /opt/secure/reboot/previousreboot.info should exist


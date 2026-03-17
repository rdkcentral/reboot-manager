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

Feature: Validate hard reboot unknown fallback mapping when no hardware category is available

	Scenario: Verify previousreboot.info defaults to NULL mapping when hardware reason is unknown
	     Given the update-prev-reboot-info binary is available
	     And no valid hardware reset reason is available from the platform
	     When the updater binary is executed
	     Then the previousreboot.info should contain source "Hard Power Reset"
	     And the previousreboot.info should contain customReason "Hardware Register - NULL"
	     And the previousreboot.info should contain otherReason "No information found"
	     And the previousreboot.info should contain reason "HARD_POWER"


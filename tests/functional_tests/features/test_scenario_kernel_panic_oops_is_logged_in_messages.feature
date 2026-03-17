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

Feature: Verify kernel panic evidence appears in messages log

	Scenario: Verify messages.txt is updated with OOPS or Kernel panic string when kernel panic is triggered
	     Given the reboot binary and update-prev-reboot-info binary are available
	     When a kernel panic reboot is triggered with source "kernel-panic" and a panic message
	     And the updater binary is executed
	     Then the file /opt/logs/messages.txt should contain "OOPS" or "Kernel panic"


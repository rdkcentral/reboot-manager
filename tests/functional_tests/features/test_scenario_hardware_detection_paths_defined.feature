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

Feature: Confirm platform hard reboot detection path constants are present in the header

	Scenario: Verify hardware detection path constants are defined in update-reboot-info.h
	     Given the update-reboot-info header file is available
	     Then the header should define AMLOGIC_SYSFS_FILE as "/sys/devices/platform/aml_pm/reset_reason"
	     And the header should define BRCM_REBOOT_FILE as "/proc/brcm/previous_reboot_reason"
	     And the header should define RTK_REBOOT_FILE as "/proc/cmdline"


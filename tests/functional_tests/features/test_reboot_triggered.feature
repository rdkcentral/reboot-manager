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

Feature: App-triggered reboot artifact verification

    Scenario: Ensure reboot metadata and flags are created for app-triggered reboot
        Given prior reboot artifacts are removed
        When reboot-manager runs with source HtmlDiagnostics
        Then /opt/secure/reboot/reboot.info should be created with source HtmlDiagnostics
        And the reason in reboot.info should be APP_TRIGGERED
        And /opt/secure/reboot/parodusreboot.info should contain a PreviousRebootInfo entry
        And /opt/logs/rebootInfo.log should include a RebootReason line
        And /opt/secure/reboot/rebootNow should exist

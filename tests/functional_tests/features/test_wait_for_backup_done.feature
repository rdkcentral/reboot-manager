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

Feature: wait_for_backup_logs_done inotify gate

    The update-prev-reboot-info binary uses an inotify-based wait for
    /tmp/.backup_logs_done before reading PreviousLogs/. This feature
    validates the three execution paths of that gate.

    Background:
        Given /tmp/stt_received flag is created
        And /opt/secure/reboot/reboot.info does not exist

    Scenario: Sentinel already present before binary starts (fast path)
        Given /tmp/.backup_logs_done already exists
        When update-prev-reboot-info binary is executed
        Then the output should contain "backup_logs sentinel already present"

    Scenario: Sentinel created during inotify wait (inotify detection)
        Given /tmp/.backup_logs_done does not exist
        And /tmp/.backup_logs_done will be created after 1 second
        When update-prev-reboot-info binary is executed
        Then the output should contain "backup_logs sentinel detected"

    Scenario: Sentinel never arrives (timeout path)
        Given /tmp/.backup_logs_done does not exist
        When update-prev-reboot-info binary is executed
        Then the output should contain "backup_logs sentinel absent after"

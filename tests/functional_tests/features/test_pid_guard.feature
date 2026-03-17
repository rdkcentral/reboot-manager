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

Feature: PID guard handling

    Scenario: Ensure PID guard file is maintained across consecutive runs
        Given the PID guard file /tmp/.rebootNow.pid does not exist
        When reboot-manager is executed for the first time
        Then the /tmp/.rebootNow.pid file should be created
        When reboot-manager is executed again with the same trigger source
        Then the second execution should succeed
        And the /tmp/.rebootNow.pid file should still exist

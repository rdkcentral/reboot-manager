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

RESULT_DIR="/tmp/l2_test_report"
mkdir -p "$RESULT_DIR"

apt-get update && apt-get install -y libjsonrpccpp-dev

/usr/local/bin/update-reboot-info &
# Run L2 Test cases
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/cyclicreboot.json functional_tests/tests/test_cyclic_reboot.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/crashmaintainence.json functional_tests/tests/test_reboot_crash_maintenance.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/systemcleanup.json functional_tests/tests/test_system_cleanup.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/rebootTest.json functional_tests/tests/test_reboot_triggered.py

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
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$SCRIPT_DIR" || exit 1
export PYTHONPATH="$SCRIPT_DIR:$PYTHONPATH"

mkdir -p "$RESULT_DIR"

mkdir -p /opt/logs
touch /opt/logs/messages.txt
apt-get update && apt-get install -y libjsonrpccpp-dev

echo "LOG.RDK.DEFAULT = INFO" >> /etc/debug.ini

# Run L2 Test cases
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/crashmaintainence.json tests/functional_tests/test/test_reboot_crash_maintenance.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/rebootTest.json tests/functional_tests/test/test_reboot_triggered.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/BootLogs.json tests/functional_tests/test/test_scenario_bootup_reboot_files_and_log_created.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/hardreboot.json tests/functional_tests/test/test_scenario_hard_reboot_unknown_defaults_to_null_mapping.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/updatehardpower.json tests/functional_tests/test/test_scenario_hard_reboot_updates_hardpower_and_previousreboot.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/kernelpanicLog.json tests/functional_tests/test/test_scenario_kernel_panic_oops_is_logged_in_messages.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/kernelreasonupdate.json tests/functional_tests/test/test_scenario_kernel_panic_oops_updates_reboot_files_via_reboot_binary.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/paroduslog.json tests/functional_tests/test/test_scenario_parodus_log_contains_reboot_reason.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/parodus_software_log_update.json tests/functional_tests/test/test_scenario_previous_parodus_file_has_software_reboot_info.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/previous_reboot_info.json tests/functional_tests/test/test_scenario_previous_reboot_info_json_format.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/string_check_msglog.json tests/functional_tests/test/test_scenario_previous_reboot_info_string_in_messages.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/previous_fields_check.json tests/functional_tests/test/test_scenario_reboot_info_log_previous_fields_present.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/swreboot_classify_matrix.json tests/functional_tests/test/test_scenario_soft_reboot_category_classification_matrix.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/swreboot_logupdate.json tests/functional_tests/test/test_scenario_soft_reboot_updates_reboot_log_and_previous_reboot_info.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/update_log_files.json tests/functional_tests/test/test_scenario_update_prev_reboot_generates_previous_files_and_flags.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/stt_flag_test.json tests/functional_tests/test/test_scenario_update_prev_reboot_service_flow_after_stt_flag.py
pytest --json-report --json-report-summary --json-report-file $RESULT_DIR/stt_flag_skip.json tests/functional_tests/test/test_scenario_update_prev_reboot_skips_when_flags_missing.py


# Nofence hardware-in-the-loop testing
Hardware in the loop testing is performed during the GitLab build pipeline. 

## Pipeline
See .gitlab-ci.yml for configuration.

## Test runner
The test runner is a Python script that does the generic setup of dependencies and test result checking. When executed by e.g. ```python3 tests-hw/nofence-tester/runner.py x25_basic_test```, the run function of x25_basic_test.py in the tests folder is executed, and all required dependencies are provided as a dictionary.

## Reporting results
Results are reported to the GitLab CI system through a junit XML file, which is handled by the report.py script. 

TODO - Implement uploading additional results by creating an output folder to be uploaded as an artifact. 

## Libraries

### ocd
The ocd library wraps openocd and provides access to flashing a target image, in addition to run control and opening RTT communication channels.

### bluefence
The bluefence library implements BLE communication with Nofence collars. Only scanning and getting scan data is supported as of now. 

TODO - Implement full communication. 
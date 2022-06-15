# Nofence hardware-in-the-loop testing
Hardware in the loop testing is performed during the GitLab CI/CD pipeline. 
See https://docs.gitlab.com/ee/ci/quick_start/

## Pipeline
The test is designed to be run as a part of the GitLab CI/CD pipeline on a dedicated runner setup. 
See .gitlab-ci.yml for configuration.

## Runner setup
The test scripts runs on Linux Debian-based host servers, but other platforms could be used as long as the libraries required for the tests are supported. Raspberry Pi has been used during development. Ubuntu should be considered to have access to more recent releases of dependencies. Follow guidelines on https://docs.gitlab.com/runner/install/ for setting up the GitLab Runner. 

The GitLab Runner is capable of serving multiple runners on a single host machine. Care must be taken when allowing multiple runners to run on the same host. Each runner instance should only have access to a specific set of connected hardware dependencies to avoid confusion. It is possible to share resources (e.g. beacon hardware), which would require some sort of resource sharing API to be implemented. 

**TODO: There needs to be mechanisms in place for uniquely identifying the exact hardware (e.g. USB serial number of debugger or serial port device path of UART connected hardware) associated with each runner. This must be used when establishing connections to the various hardware dependencies to not risk interfering with other tests in progress, and assure the test is operating as intended. This should be tied to the registered runner instances in some way, and not the job/pipeline itself.**

Each runner instance need to be registered to be accessible for the pipeline: https://docs.gitlab.com/runner/register/

Use Shell executor for runners. Using e.g. docker would require special provisions for access to hardware devices. 

Runner instances can differ with regards to capabilities. Use tags for runner instances to identify defined sets of functionality. These tags can be referenced in the pipeline setup. https://docs.gitlab.com/ee/ci/runners/configure_runners.html#use-tags-to-control-which-jobs-a-runner-can-run

## Test framework
The test framework a Python script that does the generic setup of dependencies and test result checking. When executed by e.g. ```python3 tests-hw/nofence-tester/runner.py x25_basic_test```, the run function of x25_basic_test.py in the tests folder is executed, and all required dependencies are provided as a dictionary.

## Reporting results
Results are reported to the GitLab CI system through a junit XML file, which is handled by the report.py script. 

**TODO: Implement uploading additional results by creating an output folder to be uploaded as an artifact. Results can also be pushed to a database for generating statistics from the testing, e.g. power consumption tests.**

## Libraries

### ocd
The ocd library wraps openocd and provides access to flashing a target image, in addition to run control and opening RTT communication channels. This is currently setup to use OpenOCD and the Raspberry Pi SPI module for SWD communication. To modify this for running on a setup using different tool, look in the openocd_nrf52840.cfg file and consult with OpenOCD documentation. 

### bluefence
The bluefence library implements BLE communication with Nofence collars. Only scanning and getting scan data is supported as of now. This uses the BluePy library and requires bluez stack. 

### nfdiag
The nfdiag library implements the command/response handler used by the Diagnostics module of the firmware. The Diagnostics module can be accessed through either the NUS BLE service or RTT over SWD debug interface. 

## Test plan suggestions
See [test plan suggestions](doc/test_plan.md) for suggestions of initial minimal test setup.
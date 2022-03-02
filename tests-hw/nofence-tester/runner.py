import sys
import os
import argparse
import logging

logging.basicConfig(encoding='utf-8', level=logging.DEBUG)

# Parse input arguments
parser = argparse.ArgumentParser(description='Nofence HW-in-the-loop test runner')
parser.add_argument('testplan', help='Name of testplan to run (excluding file ending .json and path). Must be found in the tests subfolder.')
args = parser.parse_args()

# Build and add search paths
base_path = os.path.dirname(os.path.abspath(__file__))

lib_path = os.path.join(base_path, "lib")
sys.path.append(lib_path)

test_path = os.path.join(base_path, "tests")
sys.path.append(test_path)

# TODO - Interpret test plan and config
test_plan = os.path.join(os.path.join(base_path, "tests"), args.testplan + ".json")
if not os.path.exists(test_plan):
    raise Exception("Could not find specified testplan: " + str(args.testplan) + " (file expected to be found " + str(test_plan) + ")")

# Setup dependencies
from ocd import OCD
ocd = OCD()

from bluefence import BlueFence
ble = BlueFence()

tests_passed = True

try:
    # TODO - Run test plan and collect results

    from tests import X25

    build_path = os.path.join(os.path.join(os.path.join(base_path, ".."), ".."), "build")
    zephyr_path = os.path.join(build_path, "zephyr")
    if not X25.flash.run(ocd, os.path.join(zephyr_path, "merged.hex")):
        tests_passed = False

    if not X25.ble_scan.run(ble):
        tests_passed = False
    
except Exception as e:
    print(e)
    tests_passed = False
finally:
    # Tear down dependencies
    del ocd

# TODO - Upload results

if not tests_passed:
    sys.exit(1)
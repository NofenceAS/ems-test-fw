import sys
import os
import argparse
import logging

logging.basicConfig(encoding='utf-8', level=logging.DEBUG)

# Parse input arguments
parser = argparse.ArgumentParser(description='Nofence HW-in-the-loop test runner')
parser.add_argument('testscript', help='Name of test script to run (excluding file ending .py and path). Must be found in the tests subfolder.')
args = parser.parse_args()

# Build and add search paths
base_path = os.path.dirname(os.path.abspath(__file__))

lib_path = os.path.join(base_path, "lib")
sys.path.append(lib_path)

test_path = os.path.join(base_path, "tests")
sys.path.append(test_path)

# Check if test exists
import tests
test_to_run = None
if (args.testscript in dir(tests)) and not args.testscript.startswith("_"):
    test_to_run = getattr(tests, args.testscript)
if not test_to_run:
    raise Exception("Unable to find specified test: " + (args.testscript))

# Setup dependencies
dep = {}

from report import Report
dep["report"] = Report("report.xml")

from ocd import OCD
dep["ocd"] = OCD()

dep["buildpath"] = os.path.join(os.path.join(os.path.join(base_path, ".."), ".."), "build")

# Run test
passed = True
try:
    passed = test_to_run.run(dep)
except Exception as e:
    print(e)
    passed = False
finally:
    report_passed = dep["report"].build_report()
    if not report_passed:
        passed = False
        
    del dep["report"]

    del dep["ocd"]

if not passed:
    sys.exit(1)
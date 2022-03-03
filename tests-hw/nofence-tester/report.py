import os
from junit_xml import TestSuite, TestCase
import datetime

class Report:
    def __init__(self, filepath):
        self._filepath = filepath
        base_path = os.path.dirname(os.path.abspath(self._filepath))
        try:
            os.makedirs(base_path)
        except:
            pass
        
        self._f = open(self._filepath, "w")

        self._testsuites = []
    
    def create_testsuite(self, name):
        testsuite = {}
        testsuite["cases"] = []
        testsuite["name"] = name
        testsuite["timestamp"] = datetime.datetime.now()

        self._testsuites.append(testsuite)
        return testsuite
    
    def start_testcase(self, testsuite, name):
        testcase = {}
        testcase["name"] = name
        testcase["timestamp"] = datetime.datetime.now()
        testcase["elapsed"] = None
        testcase["completed"] = False
        
        testsuite["cases"].append(testcase)

        return testcase
    
    def end_testcase(self, testcase, fail_message=None, fail_output=None):
        testcase["elapsed"] = (datetime.datetime.now() - testcase["timestamp"]).total_seconds()
        testcase["fail_message"] = fail_message
        testcase["fail_output"] = fail_output
        testcase["completed"] = True
    
    def build_report(self):
        all_is_passed = True

        suites = []
        for suite in self._testsuites:
            cases = []
            for case in suite["cases"]:
                t = TestCase(case["name"], elapsed_sec=case["elapsed"], timestamp=case["timestamp"].isoformat())
                if not case["completed"]:
                    all_is_passed = False
                    t.add_failure_info(message="Test not completed..")
                elif case["fail_message"]:
                    all_is_passed = False
                    t.add_failure_info(message=case["fail_message"], output=case["fail_output"])
                cases.append(t)

            suites.append(TestSuite(suite["name"], test_cases=cases, timestamp=suite["timestamp"].isoformat()))

        self._f.write(TestSuite.to_xml_string(suites))
        self._f.close()
    
        return all_is_passed
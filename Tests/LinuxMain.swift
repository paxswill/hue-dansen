import XCTest

import CaramelLightsTests
import HueDtlsTests

var tests = [XCTestCaseEntry]()
tests += CaramelLightsTests.allTests()
tests += HueDtlsTests.allTests()
XCTMain(tests)

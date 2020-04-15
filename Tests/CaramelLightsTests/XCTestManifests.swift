import XCTest

#if !canImport(ObjectiveC)
public func allTests() -> [XCTestCaseEntry] {
    return [
        testCase(CaramelLightsTests.allTests),
    ]
}
#endif

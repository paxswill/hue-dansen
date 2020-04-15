import XCTest
@testable import CaramelLights

final class CaramelLightsTests: XCTestCase {
    func testExample() {
        // This is an example of a functional test case.
        // Use XCTAssert and related functions to verify your tests produce the correct
        // results.
        XCTAssertEqual(CaramelLights().text, "Hello, World!")
    }

    static var allTests = [
        ("testExample", testExample),
    ]
}

import XCTest
@testable import HueDtls

final class HueDtlsTests: XCTestCase {
    func test_PskIdentity_init_hex() {
        let identity = PskIdentity("PSK Identity", withHex: "0xabcdef")
        XCTAssertEqual(identity.identity, "PSK Identity")
        XCTAssertEqual(identity.psk.bytes, [171, 205, 239])
    }
    
    func test_Array_init_hex() {
        let bytes = Array<UInt8>(hex: "0xb1b1b2b2")
        XCTAssertEqual(bytes, [177,177,178,178])
    }

    static var allTests = [
        ("test_Array_init_hex", test_Array_init_hex),
    ]
}

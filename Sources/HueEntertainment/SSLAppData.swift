
import OpenSSL

// Normally we could use SSL_[get,set]_app_data(), but it's defined as a macro
// which cannot be imported into Swift. So I'm defining so Swiftier functions
// in here to handle this usage.

let SSL_EX_APP_INDEX: Int32 = 0

// Using OpaquePointers in here to make the OpenSSL 1.0.0 -> 1.1.0 transition
// easier

func setSSLAppData<Instance: AnyObject>(_ ssl: OpaquePointer, data: Instance) {
    // Clear first to properly release any data that was set previously
    clearSSLAppData(ssl)
    let unmanaged = Unmanaged<Instance>.passRetained(data)
    let opaque = unmanaged.toOpaque()
    SSL_set_ex_data(.make(optional: ssl), SSL_EX_APP_INDEX, opaque)
}

func clearSSLAppData(_ ssl: OpaquePointer) {
    guard let opaque = SSL_get_ex_data(.make(optional: ssl), SSL_EX_APP_INDEX) else {
        return
    }
    let unmanaged = Unmanaged<AnyObject>.fromOpaque(opaque)
    unmanaged.release()
}

func getSSLAppData<Instance: AnyObject>(_ ssl: OpaquePointer) -> Instance? {
    guard let opaqueAppData = SSL_get_ex_data(.make(optional: ssl), SSL_EX_APP_INDEX) else {
        return nil
    }
    return Unmanaged<Instance>.fromOpaque(opaqueAppData).takeUnretainedValue()
}

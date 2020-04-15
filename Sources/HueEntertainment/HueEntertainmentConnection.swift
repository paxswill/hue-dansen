import Foundation
import Socket
import OpenSSL
import CryptoSwift
import Logging

let log = Logger(label: "com.paxswill.caramellights.hue-connection")

enum HueEntertainmentConnectionError: Error {
    case hostNotFound(host: String)
    case openSSLError(sslError: Int = -1)
}

// Using a class instead of struct so we can wrap these in Unmanaged easier
class PskIdentity {
    let identity: String
    let psk: Data
    init(_ identity: String, withHex hexPsk: String) {
        self.identity = identity
        self.psk = Data(hex: hexPsk)
    }
}

// TODO: fix up the UnsafeMutablePointer<SSL> to also be able to handle
// OpaquePointer types as well to handle OpenSSL 1.1.0
func pskCallback(_ ssl: OpaquePointer?, _ hint: UnsafePointer<CChar>?, _ identity: UnsafeMutablePointer<CChar>?, _ maxIdentityLength: UInt32, _ psk: UnsafeMutablePointer<CUnsignedChar>?, _ maxPskLength: UInt32) -> UInt32 {
    log.trace("PSK callback entered.")
    // As in other places, SSL_get_app_data is a macro, so we have to
    // reimplement the macro ourselves
    guard let unmanagedID: PskIdentity = getSSLAppData(.make(optional: ssl)!) else {
        // No PSK defined, so we will fail the handshake
        log.warning("Unable to get an identity from SSL app data.")
        return 0
    }
    // Require that there is <, not <= as we need a terminating null byte for
    // the identity
    guard unmanagedID.identity.utf8.count < maxIdentityLength else {
        log.error("Only \(maxIdentityLength) bytes available for the identity, while \(unmanagedID.identity.utf8.count) are needed.")
        return 0;
    }
    guard unmanagedID.psk.count <= maxPskLength else {
        log.error("Only \(maxPskLength) bytes available for the PSK, while \(unmanagedID.psk.count) are needed.")
        return 0;
    }
    // Cast the output buffers to usable types
    let identityBuffer = UnsafeMutableBufferPointer<CChar>(start: identity!, count: Int(maxIdentityLength))
    let pskBuffer = UnsafeMutableBufferPointer<CUnsignedChar>(start: psk!, count: Int(maxPskLength))
    // Copy the data from unmanagedID to the appropriate buffers
    _ = unmanagedID.identity.utf8CString.withUnsafeBufferPointer { sourceBuffer in
        identityBuffer.initialize(from: sourceBuffer)
    }
    log.trace("Copying PSK bytes for identity \(unmanagedID.identity).")
    return UInt32(unmanagedID.psk.copyBytes(to: pskBuffer))
}

class HueEntertainmentConnection {
    // If only supporting OpenSSL 1.0.0, we can use typed UnsafeMutablePointers,
    // But OpenSSL 1.1.0 made them all opaque, so we get OpaquePointers instead.
    // TODO: convert sslContext to a computed type property
    let sslContext: OpaquePointer
    var udpSocket: Socket?
    var ssl: OpaquePointer? {
        didSet {
            // Transfer the app data around as needed
            guard identity != nil else {
                return
            }
            // Rely on self.identity keeping a strong reference
            if (oldValue != nil) {
                clearSSLAppData(oldValue!)
            }
            if (ssl != nil) {
                setSSLAppData(ssl!, data: identity!)
            }
        }
    }
    var bio: OpaquePointer?
    var identity: PskIdentity? {
        didSet {
            // If we don't have an SSL connection open, we can't set app data for it.
            guard ssl != nil else {
                return
            }
            // Clean up any old data
            if (oldValue != nil) {
                clearSSLAppData(ssl!)
            }
            guard identity != nil else {
                return
            }
            setSSLAppData(ssl!, data: identity!)
        }
    }
    
    convenience init(identity: String, pskHex: String) throws {
        try self.init()
        self.identity = PskIdentity(identity, withHex: pskHex)
    }
    
    init() throws {
        guard let sslContext = OpaquePointer.make(optional: SSL_CTX_new(DTLS_client_method())) else {
            throw HueEntertainmentConnectionError.openSSLError()
        }
        self.sslContext = sslContext
        /* Well, this is annoying.
         * OpenSSL had a number changes between 1.0 and 1.1. The most
         * salient change to Swift was that a bunch of types were made
         * opaque (see SSLPointerTricks). Another change was
         * SSL_[CTX_]_set_options (and related things) were change from
         * preprocessor macros to functions.
         *
         * Swift doesn't handle function-like macros, it skips them.
         *
         * It looks like it's impossible to support both 1.0 and 1.1 in
         * the same code base. The OpenSSL bridge I'm using has a partial
         * wrapper around SSL_CTX_set_options, but it uses a hard coded set of
         * supported protocols.
         * Instead of explicitly disabling all other protocols, I'm just going
         * to leave them all enabled at their defaults (and hope DTLSv1.2 is
         * enabled)
         */
        // Hue entertainment only uses DTLS 1.2
        // SSL_CTX_set_min_proto_version was added in OpenSSL 1.1.0, so no
        // dice for OpenSSL 1.0.0
        // SSL_CTX_set_min_proto_version(context, DTLS1_2_VERSION);
        //let sslOptions: CLong = SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | SSL_OP_NO_DTLSv1;
        //SSL_CTX_ctrl(.make(optional: self.sslContext), SSL_CTRL_OPTIONS, sslOptions, nil)
        // Hue entertainment supports exactly one cipher
        let cipherList = "PSK-AES128-GCM-SHA256"
        _ = cipherList.withCString { (cipherListC) -> Result<Any, HueEntertainmentConnectionError> in
            SSL_CTX_set_cipher_list(.make(optional: self.sslContext), cipherListC)
            return .success(0)
        }
        SSL_CTX_set_psk_client_callback(.make(optional: self.sslContext), pskCallback)
    }
    
    func connect(_ host: String, port: Int32 = 2100) throws {
        // Use a mutable variable for address as it's passed to an inout later
        guard var address = Socket.createAddress(for: host, on: port) else {
            throw HueEntertainmentConnectionError.hostNotFound(host: host)
        }
        udpSocket = try Socket.create(family: address.family, type: .datagram, proto: .udp)
        try udpSocket!.connect(to: host, port: port)
        // Wrap the UDP socket in a BIO for OpenSSL
        bio = OpaquePointer(BIO_new_dgram(udpSocket!.socketfd, BIO_NOCLOSE))
        guard bio != nil else {
            // TODO cleanup socket
            throw HueEntertainmentConnectionError.openSSLError(sslError: -1)
        }
        let bioSetConnectedError = BIO_ctrl(.make(optional: bio), BIO_CTRL_DGRAM_SET_CONNECTED, 0, &address)
        guard bioSetConnectedError == 0 else {
            // TODO cleanup BIO, socket
            throw HueEntertainmentConnectionError.openSSLError(sslError: bioSetConnectedError)
        }
        // Open the DTLS connection
        ssl = OpaquePointer(SSL_new(.make(optional: sslContext)))
        guard ssl != nil else {
            // TODO cleanup BIO, socket
            throw HueEntertainmentConnectionError.openSSLError(sslError: -1)
        }
        SSL_set_bio(.make(optional: ssl), .make(optional: bio), .make(optional: bio))
        let sslConnectError = SSL_connect(.make(optional: ssl))
        guard sslConnectError == 0 else {
            // TODO cleanup BIO, socket
            throw HueEntertainmentConnectionError.openSSLError(sslError: Int(sslConnectError))
        }
        // Set timeouts *after* the connection is established
        var timeout: timeval = timeval(tv_sec: 2, tv_usec: 0)
        let bioTimeoutError = BIO_ctrl(.make(optional: bio), BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout)
        guard bioTimeoutError == 0 else {
            throw HueEntertainmentConnectionError.openSSLError(sslError: bioTimeoutError)
        }
    }
}

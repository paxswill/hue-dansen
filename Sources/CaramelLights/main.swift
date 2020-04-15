import Logging
import ArgumentParser

import Color
import HueEntertainment

// Color literals. This is crazy.
let colors: [Color<Float>] = [
    #colorLiteral(red: 0.7450980544, green: 0.1568627506, blue: 0.07450980693, alpha: 1),
    #colorLiteral(red: 0.3411764801, green: 0.6235294342, blue: 0.1686274558, alpha: 1),
    #colorLiteral(red: 0.01680417731, green: 0.1983509958, blue: 1, alpha: 1),
    #colorLiteral(red: 1, green: 0.5781051517, blue: 0, alpha: 1),
]

struct CaramelLights: ParsableCommand {
    static let configuration: CommandConfiguration = CommandConfiguration(commandName: "caramellights", abstract: "Spam the Hue bridge with Hue Entertainment colros in time with Caramelldansen")
    
    @Option(name: [.customShort("b"), .customLong("bridge")])
    var hueBridge: String
    
    @Option(name: .shortAndLong)
    var identity: String
    
    @Option(name: .shortAndLong)
    var psk: String
    
    @Option(name: .shortAndLong)
    var duration: UInt?
    
    @Option(name: [.customLong("light"), .short])
    var lightIDs: [UInt]
    
    mutating func validate() throws {
        guard !hueBridge.isEmpty else {
            // TODO: add actual validation that it's a hostname or IP
            throw ValidationError("Either a hostname or IP address must be given for the Hue bridge.")
        }
        guard !lightIDs.isEmpty else {
            throw ValidationError("At least one Light ID must be given.")
        }
        guard psk.count == 32 else {
            throw ValidationError("The PSK must be exactly 32 characters.")
        }
        let hexCharacters = "0123456789abcdef"
        guard psk.lowercased().allSatisfy({ hexCharacters.contains($0) }) else {
            throw ValidationError("The PSK must only contain hexadecimal characters.")
        }
    }
    
    func run() throws {
        let connection = try HueEntertainmentConnection(identity: identity, pskHex: psk)
        try connection.connect(hueBridge)
    }
}

LoggingSystem.bootstrap(StreamLogHandler.standardError)
CaramelLights.main()

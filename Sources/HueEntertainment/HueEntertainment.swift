import Foundation

class HueEntertainmentUpdater: Thread {
    let host: String
    let connection: HueEntertainmentConnection
    
    init(host: String, identity: String, pskHex: String) throws {
        self.host = host
        try connection = HueEntertainmentConnection(identity: identity, pskHex: pskHex)
        super.init()
        name = "com.paxswill.caramellights.hue-entertainment-updater"
    }
    
    override func main() {
        do {
            try connection.connect(host)
        } catch {
            cancel()
            return
        }
        let loop = RunLoop.current
        
        loop.run()

    }
    
    
}

class HueEntertainment {
    
}

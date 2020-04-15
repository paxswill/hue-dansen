import Foundation
import Matrix
//fileprivate typealias Real = BinaryFloatingPoint

protocol ChromaticityPoint {
    associatedtype NumberType: Real
    // CIE x (chromaticity x)
    var x: NumberType { get }
    // CIE y (chromaticity y)
    var y: NumberType { get }
    // CIE Y, aka luminance
    var brightness: NumberType { get }
}

extension ChromaticityPoint {
    // CIE Z, somewhat equal to blue-ish
    var Z: NumberType { get {return (brightness / y) * (1 - x - y) } }
    
    // CIE X
    var X: NumberType { get { return (brightness / y) * x } }
    
    var Y: NumberType { get { return brightness } }
}

public struct WhitePoint<T: Real>: ChromaticityPoint {
    typealias NumberType = T
    let x, y: NumberType
    
    var brightness: NumberType { get { return 1.0 } }
    
    public static var D65: Self<NumberType> {
        get { Self<NumberType>(x: NumberType(0.31271), y: NumberType(0.32902)) }
    }
    
    public static var E: Self<NumberType> {
        get { Self<NumberType>(x: NumberType(1.0 / 3.0), y: NumberType(1.0 / 3.0)) }
    }
}

func raise(_ base: Float, _ exponent: Float) -> Float {
    return powf(base, exponent)
}

func raise(_ base: Double, _ exponent: Double) -> Double {
    return pow(base, exponent)
}

#if (arch(x86_64) || arch(i386)) && !targetEnvironment(simulator)
func raise(_ base: Float80, _ exponent: Float80) -> Float80 {
    return powl(base, exponent)
}
#endif


public struct RGBColorspace<NumberType: Real> {
    public typealias GammaFunction = (NumberType) -> NumberType
    public struct Point {
        let x: NumberType
        let y: NumberType
    }
    let r, g, b: Point
    let white: WhitePoint<NumberType>
    let gamma: GammaFunction

    static func gammaIdentity(_ v: NumberType) -> NumberType {
        return v
    }

    static func gammaTwoTwo(_ v: NumberType) -> NumberType {
        // actually the inverse, for decoding RGB to XYZ
        // Also leaving A = 1
        return NumberType(pow(Double(v), 1/2.2))
        //return raise(v, 1/2.2)
    }
    
    public func forwardMatrix() -> [[NumberType]] {
        // For converting RGB to XYZ
        let xyz = [r, g, b].map { chromaCoordinate in
            return [
                chromaCoordinate.x / chromaCoordinate.y,
                1.0,
                (1 - chromaCoordinate.x - chromaCoordinate.y) / chromaCoordinate.y,
            ]
        }
        let xyzInverse = invert(xyz)
        let S = matrixMultiply(xyzInverse, [[white.X, white.Y, white.Z]])
        let M = zip(S[0], xyz).map { (sSub, column) in
            return column.map { element in element * sSub }
        }
        return M
    }
    
    public func inverseMatrix() -> [[NumberType]] {
        // For converting XYZ to RGB
        return invert(forwardMatrix())
    }
    
    public static var sRGB: Self<NumberType> {
        get {
            let r = Point(x: 0.64, y: 0.33)
            let g = Point(x: 0.30, y: 0.60)
            let b = Point(x: 0.15, y: 0.06)
            // TODO: implement the actual sRGB gamma correction
            return Self<NumberType>(r: r, g: g, b: b, white: WhitePoint<NumberType>.D65, gamma: Self.gammaTwoTwo)
        }
    }
    
    public static var cieRGB: Self<NumberType> {
        get {
            let r = Point(x: 0.7347, y: 0.2653)
            let g = Point(x: 0.2738, y: 0.7174)
            let b = Point(x: 0.1666, y: 0.0089)
            return Self<NumberType>(r: r, g: g, b: b, white: WhitePoint<NumberType>.E, gamma: Self.gammaIdentity)
        }
    }
}

public struct Color<T: Real>: ChromaticityPoint {
    public typealias NumberType = T
    // CIE x (chromaticity x)
    public let x: NumberType
    // CIE y (chromaticity y)
    public let y: NumberType
    // CIE Y, aka luminance
    public let brightness: NumberType
    
    public init(x: NumberType, y: NumberType, brightness: NumberType?) {
        self.x = x
        self.y = y
        if brightness != nil {
            self.brightness = brightness!
        } else {
            self.brightness = NumberType(1.0)
        }
    }
    
    public init<U: FixedWidthInteger & UnsignedInteger>(r: U, g: U, b: U) {
        self.init(r: r, g: g, b: b, colorspace: RGBColorspace<NumberType>.sRGB)
    }
    
    public init<U: FixedWidthInteger & UnsignedInteger>(r: U, g: U, b: U, colorspace: RGBColorspace<NumberType>) {
        let maxRange: NumberType = NumberType(U.max)
        // Special case white-gray-black
        if r == g && g == b {
            self.init(x: colorspace.white.x, y: colorspace.white.y, brightness: NumberType(r) / maxRange)
        } else {
            // convert the integer values to floating point values 0.0-1.0
            let rgb: [NumberType] = [r, g, b].map { val in
                    NumberType(val) / maxRange
                }
            let linearRGB = rgb.map(colorspace.gamma)
            let forwardMatrix: [[NumberType]] = colorspace.forwardMatrix()
            let XYZ: [[NumberType]] = matrixMultiply(forwardMatrix, [linearRGB])
            // convert X and Z to x and y
            let total: NumberType = XYZ[0].reduce(NumberType.zero, +)
            var x: NumberType = NumberType.zero
            var y: NumberType = NumberType.zero
            if total == NumberType.zero {
                x = colorspace.white.x
                y = colorspace.white.y
            } else {
                x = XYZ[0][0] / total
                y = XYZ[0][1] / total
            }
            self.init(x: x, y: y, brightness: XYZ[0][1])
        }
    }
}

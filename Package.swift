// swift-tools-version:5.2
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "CaramelLights",
    products: [
        // Products define the executables and libraries produced by a package, and make them visible to other packages.
        .library(
            name: "CaramelLights",
            targets: ["CaramelLights"]),
        .library(
            name: "HueEntertainment",
            targets: ["HueEntertainment"]),
        .library(
            name: "Color",
            targets: ["Color"]),
        .library(
            name: "Matrix",
            targets: ["Matrix"]),
    ],
    dependencies: [
        // Dependencies declare other packages that this package depends on.
        // .package(url: /* package url */, from: "1.0.0"),
        .package(name: "OpenSSL", url: "https://github.com/IBM-Swift/OpenSSL.git", from: "2.2.1"),
        .package(name: "Socket", url: "https://github.com/IBM-Swift/BlueSocket.git", from: "1.0.52"),
        .package(name: "CryptoSwift", url: "https://github.com/krzyzanowskim/CryptoSwift.git", from: "1.3.1"),
        .package(url: "https://github.com/apple/swift-log.git", from: "1.0.0"),
    ],
    targets: [
        // Targets are the basic building blocks of a package. A target can define a module or a test suite.
        // Targets can depend on other targets in this package, and on products in packages which this package depends on.
        .target(
            name: "CaramelLights",
            dependencies: []),
        .target(
            name: "HueEntertainment",
            dependencies: [
                "OpenSSL",
                "Socket",
                "CryptoSwift",
                .product(name: "Logging", package: "swift-log"),
                "Color",
        ]),
        .target(
            name: "Color",
            dependencies: [
                "Matrix",
                .product(name: "Logging", package: "swift-log"),
        ]),
        .target(
            name: "Matrix",
            dependencies: [
                .product(name: "Logging", package: "swift-log"),
        ]),
        .testTarget(
            name: "CaramelLightsTests",
            dependencies: ["CaramelLights"]),
        .testTarget(
            name: "HueEntertainmentTests",
            dependencies: ["HueEntertainment"]),
    ]
)

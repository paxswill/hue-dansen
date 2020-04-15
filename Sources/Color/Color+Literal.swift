extension Color: _ExpressibleByColorLiteral where NumberType == Float{
    public init(_colorLiteralRed red: Float, green: Float, blue: Float, alpha: Float) {
        self.init(red: red, green: green, blue: blue)
    }
}

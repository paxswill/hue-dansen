/* If you're doing a lot of these operations, I'd recommend using an actual
 * library for them (like Accelerate on Apple platforms) as these
 * implementations are naive and inefficient.
 */

public typealias Real = BinaryFloatingPoint

func dot<Number: Real>(_ left: [Number], _ right: [Number]) -> Number {
    return zip(left, right).map(*).reduce(0, +)
}

public func matrixMultiply<Number: Real>(_ left: [[Number]], _ right: [[Number]]) -> [[Number]] {
    assert(left.count == right[0].count)
    let outputColumns = right.count
    let outputRows = left[0].count
    var output: [[Number]] = Array(repeating: Array(repeating: Number.zero, count: outputRows), count: outputColumns)
    for column in 0..<outputColumns {
        for row in 0..<outputRows {
            let leftRow = left.map { leftColumn in leftColumn[row] }
            output[column][row] = dot(leftRow, right[column])
        }
    }
    return output
}

public func determinant<Number: Real>(_ matrix: [[Number]]) -> Number {
    for column in matrix {
        assert(matrix.count == column.count)
    }
    if matrix.count == 0 {
        return Number.zero
    } else if matrix.count == 1 {
        return matrix[0][0]
    } else if matrix.count == 2 {
        return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0]
    } else {
        var intermediate: Number = Number.zero
        var sign: Number = 1
        for column in 0..<matrix.count {
            var submatrix: [[Number]] = []
            for subColumn in 0..<matrix.count {
                if subColumn == column {
                    continue
                }
                submatrix.append(Array<Number>(matrix[subColumn][1..<matrix.count]))
            }
            intermediate += sign * matrix[column][0] * determinant(submatrix)
            sign *= -1
        }
        return intermediate
    }
}

func minors<Number: Real>(_ matrix: [[Number]]) -> [[Number]] {
    var results: [[Number]] = []
    // I just close my eyes and ignore the complexity
    for column in 0..<matrix.count {
        var innerResults: [Number] = []
        for row in 0..<matrix.count {
            var subMatrix: [[Number]] = []
            for subColumn in 0..<matrix.count {
                if subColumn == column {
                    continue
                }
                var innerSubMatrix: [Number] = []
                for subRow in 0..<matrix.count {
                    if subRow == row {
                        continue
                    }
                    innerSubMatrix.append(matrix[subColumn][subRow])
                }
                subMatrix.append(innerSubMatrix)
            }
            innerResults.append(determinant(subMatrix))
        }
        results.append(innerResults)
    }
    return results
}

public func invert<Number: Real>(_ matrix: [[Number]]) -> [[Number]] {
    // At first it's just the matrix of minors
    var cofactors = minors(matrix)
    var sign: Number = 1
    // And these two loops make it a matrix of cofactors
    for column in 0..<matrix.count {
        for row in 0..<matrix.count {
            cofactors[column][row] *= sign
            sign *= -1
        }
    }
    // Transpose
    var transposed: [[Number]] = []
    for column in 0..<matrix.count {
        var transposedColumn: [Number] = []
        for row in 0..<matrix.count {
            transposedColumn.append(cofactors[row][column])
        }
        transposed.append(transposedColumn)
    }
    // Instead of the dedicated determinant function, we're using some of the
    // work we already did
    let determinant = matrix.enumerated().reduce(into: [Number](), { (terms, enumerated) in
        let (index, column) = enumerated
        terms.append(column[0] * cofactors[index][0])
    }).reduce(Number.zero, +)
    let coefficient: Number = Number(1.0) / Number(determinant)
    let elements: [[Number]] = transposed.map { column in column.map { element in Number(element) } }
    return elements.map { column in column.map { element in coefficient * element } }
}

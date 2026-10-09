#include "../include/interpreter.hpp"
#include <stdexcept>

std::string Interpreter::run(Program* program) {
    variables.clear();
    out.str("");
    out.clear();

    for (auto& stmt : program->statements) {
        execute(stmt.get());
    }

    return out.str();
}

void Interpreter::execute(Statement* stmt) {
    if (auto* decl = dynamic_cast<MatrixDeclaration*>(stmt)) {
        variables[decl->name] = toStored(evaluate(decl->value.get()));
    } else if (auto* assign = dynamic_cast<AssignmentStatement*>(stmt)) {
        variables[assign->name] = toStored(evaluate(assign->value.get()));
    } else if (auto* printStmt = dynamic_cast<PrintStatement*>(stmt)) {
        print(evaluate(printStmt->expr.get()));
    }
}

// Variables always hold matrices, so a scalar value is stored as a 1x1 matrix
// (same rule as the semantic analyzer and the C++ backend)
Matrix Interpreter::toStored(const Value& v) {
    if (v.isScalar) {
        return Matrix{std::vector<double>{v.scalar}};
    }
    return v.matrix;
}

Value Interpreter::evaluate(Expression* expr) {
    if (auto* num = dynamic_cast<NumberLiteral*>(expr)) {
        return Value::fromScalar(num->value);
    }

    if (auto* id = dynamic_cast<Identifier*>(expr)) {
        return Value::fromMatrix(variables.at(id->name));
    }

    if (auto* literal = dynamic_cast<MatrixLiteral*>(expr)) {
        return Value::fromMatrix(literal->values);
    }

    if (auto* call = dynamic_cast<BuiltinCall*>(expr)) {
        if (call->name == "identity") {
            size_t n = call->args[0];
            Matrix m(n, std::vector<double>(n, 0.0));
            for (size_t i = 0; i < n; ++i) m[i][i] = 1.0;
            return Value::fromMatrix(std::move(m));
        }
        double fill = call->name == "ones" ? 1.0 : 0.0;
        return Value::fromMatrix(Matrix(call->args[0], std::vector<double>(call->args[1], fill)));
    }

    if (auto* unOp = dynamic_cast<UnaryOp*>(expr)) {
        Value operand = evaluate(unOp->operand.get());
        if (unOp->op == TokenType::MINUS) {
            return applyBinary(TokenType::MULTIPLY, Value::fromScalar(-1.0), operand);
        }
        // TRANSPOSE
        const Matrix& a = operand.matrix;
        Matrix result(a[0].size(), std::vector<double>(a.size()));
        for (size_t i = 0; i < a.size(); ++i) {
            for (size_t j = 0; j < a[0].size(); ++j) {
                result[j][i] = a[i][j];
            }
        }
        return Value::fromMatrix(std::move(result));
    }

    if (auto* binOp = dynamic_cast<BinaryOp*>(expr)) {
        Value left = evaluate(binOp->left.get());
        Value right = evaluate(binOp->right.get());
        return applyBinary(binOp->op, left, right);
    }

    throw std::runtime_error("Interpreter: unknown expression");
}

Value Interpreter::applyBinary(TokenType op, const Value& left, const Value& right) {
    // scalar (op) scalar
    if (left.isScalar && right.isScalar) {
        switch (op) {
            case TokenType::PLUS: return Value::fromScalar(left.scalar + right.scalar);
            case TokenType::MINUS: return Value::fromScalar(left.scalar - right.scalar);
            case TokenType::MULTIPLY: return Value::fromScalar(left.scalar * right.scalar);
            default: return Value::fromScalar(left.scalar / right.scalar);
        }
    }

    // matrix * matrix is a true matrix product
    if (op == TokenType::MULTIPLY && !left.isScalar && !right.isScalar) {
        const Matrix& a = left.matrix;
        const Matrix& b = right.matrix;
        size_t n = a.size(), m = b[0].size(), inner = a[0].size();
        Matrix result(n, std::vector<double>(m, 0.0));
        // i-k-j loop order walks both inputs row by row (cache friendly)
        for (size_t i = 0; i < n; ++i) {
            for (size_t k = 0; k < inner; ++k) {
                double aik = a[i][k];
                for (size_t j = 0; j < m; ++j) {
                    result[i][j] += aik * b[k][j];
                }
            }
        }
        return Value::fromMatrix(std::move(result));
    }

    // Everything else is element-wise: matrix (op) matrix, or a scalar broadcast
    const Matrix& shape = left.isScalar ? right.matrix : left.matrix;
    Matrix result(shape.size(), std::vector<double>(shape[0].size()));
    for (size_t i = 0; i < shape.size(); ++i) {
        for (size_t j = 0; j < shape[0].size(); ++j) {
            double x = left.isScalar ? left.scalar : left.matrix[i][j];
            double y = right.isScalar ? right.scalar : right.matrix[i][j];
            switch (op) {
                case TokenType::PLUS: result[i][j] = x + y; break;
                case TokenType::MINUS: result[i][j] = x - y; break;
                case TokenType::MULTIPLY: result[i][j] = x * y; break;
                default: result[i][j] = x / y; break;
            }
        }
    }
    return Value::fromMatrix(std::move(result));
}

// Same format as the generated C++ (std::cout defaults), so both backends
// produce byte-identical output
void Interpreter::print(const Value& v) {
    if (v.isScalar) {
        out << v.scalar << "\n";
        return;
    }
    for (const auto& row : v.matrix) {
        for (size_t j = 0; j < row.size(); ++j) {
            if (j > 0) out << " ";
            out << row[j];
        }
        out << "\n";
    }
}

#include "../include/codegen.hpp"
#include <iomanip>
#include <sstream>

// Writes a double as a C++ literal that parses back to exactly the same value.
// 17 significant digits round-trip any double, and the literal always contains
// '.' or an exponent so it stays a double (5 / 2 must not become integer division).
static std::string doubleLiteral(double value) {
    std::ostringstream os;
    os << std::setprecision(17) << value;
    std::string text = os.str();
    if (text.find_first_of(".eE") == std::string::npos) {
        text += ".0";
    }
    return text;
}

std::string CodeGenerator::generate(std::unique_ptr<Program>& program) {
    output.str("");
    output.clear();
    indentLevel = 0;
    
    // Emit header and helper functions
    emitLine("#include <vector>");
    emitLine("#include <iostream>");
    emitLine("#include <iomanip>");
    emitLine("#include <type_traits>");
    emitLine("");
    emitLine("using Matrix = std::vector<std::vector<double>>;");
    emitLine("");
    
    // Helper functions
    emitLine("Matrix add(const Matrix& a, const Matrix& b) {");
    increaseIndent();
    emitLine("Matrix result(a.size(), std::vector<double>(a[0].size()));");
    emitLine("for (size_t i = 0; i < a.size(); ++i) {");
    increaseIndent();
    emitLine("for (size_t j = 0; j < a[0].size(); ++j) {");
    increaseIndent();
    emitLine("result[i][j] = a[i][j] + b[i][j];");
    decreaseIndent();
    emitLine("}");
    decreaseIndent();
    emitLine("}");
    emitLine("return result;");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    emitLine("Matrix sub(const Matrix& a, const Matrix& b) {");
    increaseIndent();
    emitLine("Matrix result(a.size(), std::vector<double>(a[0].size()));");
    emitLine("for (size_t i = 0; i < a.size(); ++i) {");
    increaseIndent();
    emitLine("for (size_t j = 0; j < a[0].size(); ++j) {");
    increaseIndent();
    emitLine("result[i][j] = a[i][j] - b[i][j];");
    decreaseIndent();
    emitLine("}");
    decreaseIndent();
    emitLine("}");
    emitLine("return result;");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    emitLine("Matrix mul(const Matrix& a, const Matrix& b) {");
    increaseIndent();
    emitLine("Matrix result(a.size(), std::vector<double>(b[0].size(), 0.0));");
    emitLine("for (size_t i = 0; i < a.size(); ++i) {");
    increaseIndent();
    emitLine("for (size_t j = 0; j < b[0].size(); ++j) {");
    increaseIndent();
    emitLine("for (size_t k = 0; k < a[0].size(); ++k) {");
    increaseIndent();
    emitLine("result[i][j] += a[i][k] * b[k][j];");
    decreaseIndent();
    emitLine("}");
    decreaseIndent();
    emitLine("}");
    decreaseIndent();
    emitLine("}");
    emitLine("return result;");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    emitLine("Matrix scale(const Matrix& a, double s) {");
    increaseIndent();
    emitLine("Matrix result(a.size(), std::vector<double>(a[0].size()));");
    emitLine("for (size_t i = 0; i < a.size(); ++i) {");
    increaseIndent();
    emitLine("for (size_t j = 0; j < a[0].size(); ++j) {");
    increaseIndent();
    emitLine("result[i][j] = a[i][j] * s;");
    decreaseIndent();
    emitLine("}");
    decreaseIndent();
    emitLine("}");
    emitLine("return result;");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    emitLine("Matrix scale(double s, const Matrix& a) {");
    increaseIndent();
    emitLine("return scale(a, s);");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    emitLine("Matrix div(const Matrix& a, double s) {");
    increaseIndent();
    emitLine("Matrix result(a.size(), std::vector<double>(a[0].size()));");
    emitLine("for (size_t i = 0; i < a.size(); ++i) {");
    increaseIndent();
    emitLine("for (size_t j = 0; j < a[0].size(); ++j) {");
    increaseIndent();
    emitLine("result[i][j] = a[i][j] / s;");
    decreaseIndent();
    emitLine("}");
    decreaseIndent();
    emitLine("}");
    emitLine("return result;");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    emitLine("Matrix transpose(const Matrix& a) {");
    increaseIndent();
    emitLine("Matrix result(a[0].size(), std::vector<double>(a.size()));");
    emitLine("for (size_t i = 0; i < a.size(); ++i) {");
    increaseIndent();
    emitLine("for (size_t j = 0; j < a[0].size(); ++j) {");
    increaseIndent();
    emitLine("result[j][i] = a[i][j];");
    decreaseIndent();
    emitLine("}");
    decreaseIndent();
    emitLine("}");
    emitLine("return result;");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    emitLine("Matrix filled(size_t r, size_t c, double v) {");
    increaseIndent();
    emitLine("return Matrix(r, std::vector<double>(c, v));");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    emitLine("Matrix identity(size_t n) {");
    increaseIndent();
    emitLine("Matrix result = filled(n, n, 0.0);");
    emitLine("for (size_t i = 0; i < n; ++i) result[i][i] = 1.0;");
    emitLine("return result;");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    emitLine("void printMatrix(const Matrix& m) {");
    increaseIndent();
    emitLine("for (size_t i = 0; i < m.size(); ++i) {");
    increaseIndent();
    emitLine("for (size_t j = 0; j < m[0].size(); ++j) {");
    increaseIndent();
    emitLine("if (j > 0) std::cout << \" \";");
    emitLine("std::cout << m[i][j];");
    decreaseIndent();
    emitLine("}");
    emitLine("std::cout << std::endl;");
    decreaseIndent();
    emitLine("}");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    emitLine("void printMatrix(double d) {");
    increaseIndent();
    emitLine("std::cout << d << std::endl;");
    decreaseIndent();
    emitLine("}");
    emitLine("");
    
    // Operator overloads for mixed operations
    emitLine("Matrix operator+(const Matrix& a, const Matrix& b) { return add(a, b); }");
    emitLine("Matrix operator+(const Matrix& a, double b) {");
    increaseIndent();
    emitLine("Matrix scalarMat(a.size(), std::vector<double>(a[0].size(), b));");
    emitLine("return add(a, scalarMat);");
    decreaseIndent();
    emitLine("}");
    emitLine("Matrix operator+(double a, const Matrix& b) {");
    increaseIndent();
    emitLine("Matrix scalarMat(b.size(), std::vector<double>(b[0].size(), a));");
    emitLine("return add(scalarMat, b);");
    decreaseIndent();
    emitLine("}");
    emitLine("Matrix operator-(const Matrix& a, const Matrix& b) { return sub(a, b); }");
    emitLine("Matrix operator-(const Matrix& a, double b) {");
    increaseIndent();
    emitLine("Matrix scalarMat(a.size(), std::vector<double>(a[0].size(), b));");
    emitLine("return sub(a, scalarMat);");
    decreaseIndent();
    emitLine("}");
    emitLine("Matrix operator-(double a, const Matrix& b) {");
    increaseIndent();
    emitLine("Matrix scalarMat(b.size(), std::vector<double>(b[0].size(), a));");
    emitLine("return sub(scalarMat, b);");
    decreaseIndent();
    emitLine("}");
    emitLine("Matrix operator*(const Matrix& a, const Matrix& b) { return mul(a, b); }");
    emitLine("Matrix operator*(const Matrix& a, double b) { return scale(a, b); }");
    emitLine("Matrix operator*(double a, const Matrix& b) { return scale(b, a); }");
    emitLine("Matrix operator/(const Matrix& a, double b) { return div(a, b); }");
    emitLine("");
    
    emitLine("int main() {");
    increaseIndent();
    
    generateProgram(program.get());
    
    emitLine("return 0;");
    decreaseIndent();
    emitLine("}");
    
    return output.str();
}

void CodeGenerator::emit(const std::string& code) {
    output << code;
}

void CodeGenerator::emitLine(const std::string& code) {
    output << getIndent() << code << "\n";
}

void CodeGenerator::increaseIndent() {
    indentLevel++;
}

void CodeGenerator::decreaseIndent() {
    if (indentLevel > 0) indentLevel--;
}

std::string CodeGenerator::getIndent() {
    return std::string(indentLevel * 2, ' ');
}

void CodeGenerator::generateProgram(Program* program) {
    for (auto& stmt : program->statements) {
        generateStatement(stmt.get());
    }
}

void CodeGenerator::generateStatement(Statement* stmt) {
    if (auto* decl = dynamic_cast<MatrixDeclaration*>(stmt)) {
        generateMatrixDeclaration(decl);
    } else if (auto* assign = dynamic_cast<AssignmentStatement*>(stmt)) {
        generateAssignment(assign);
    } else if (auto* print = dynamic_cast<PrintStatement*>(stmt)) {
        generatePrint(print);
    }
}

void CodeGenerator::generateMatrixDeclaration(MatrixDeclaration* decl) {
    std::string varName = sanitizeIdentifier(decl->name);
    emit(getIndent() + "Matrix " + varName + " = ");
    generateStoredValue(decl->value.get());
    emit(";\n");
}

void CodeGenerator::generateAssignment(AssignmentStatement* assign) {
    std::string varName = sanitizeIdentifier(assign->name);
    emit(getIndent() + varName + " = ");
    generateStoredValue(assign->value.get());
    emit(";\n");
}

void CodeGenerator::generatePrint(PrintStatement* print) {
    // printMatrix is overloaded for both matrices and scalars
    emit(getIndent() + "printMatrix(");
    generateExpression(print->expr.get());
    emit(");\n");
}

// Variables always hold matrices, so a scalar value is stored as a 1x1 matrix
void CodeGenerator::generateStoredValue(Expression* expr) {
    if (isScalarExpression(expr)) {
        emit("Matrix{std::vector<double>{");
        generateExpression(expr);
        emit("}}");
    } else {
        generateExpression(expr);
    }
}

bool CodeGenerator::isScalarExpression(Expression* expr) {
    if (dynamic_cast<NumberLiteral*>(expr)) return true;
    if (auto* binOp = dynamic_cast<BinaryOp*>(expr)) {
        return isScalarExpression(binOp->left.get()) && isScalarExpression(binOp->right.get());
    }
    if (auto* unOp = dynamic_cast<UnaryOp*>(expr)) {
        return unOp->op == TokenType::MINUS && isScalarExpression(unOp->operand.get());
    }
    // Identifiers, matrix literals and builtins are all matrices
    return false;
}

void CodeGenerator::generateExpression(Expression* expr) {
    if (auto* num = dynamic_cast<NumberLiteral*>(expr)) {
        output << doubleLiteral(num->value);
    } else if (auto* id = dynamic_cast<Identifier*>(expr)) {
        output << sanitizeIdentifier(id->name);
    } else if (auto* matrix = dynamic_cast<MatrixLiteral*>(expr)) {
        generateMatrixLiteral(matrix);
    } else if (auto* binOp = dynamic_cast<BinaryOp*>(expr)) {
        generateBinaryOp(binOp);
    } else if (auto* unOp = dynamic_cast<UnaryOp*>(expr)) {
        generateUnaryOp(unOp);
    } else if (auto* call = dynamic_cast<BuiltinCall*>(expr)) {
        generateBuiltinCall(call);
    }
}

void CodeGenerator::generateBuiltinCall(BuiltinCall* call) {
    if (call->name == "identity") {
        output << "identity(" << call->args[0] << ")";
    } else {
        output << "filled(" << call->args[0] << ", " << call->args[1] << ", "
               << (call->name == "ones" ? "1.0" : "0.0") << ")";
    }
}

void CodeGenerator::generateMatrixLiteral(MatrixLiteral* matrix) {
    output << "Matrix{";
    for (size_t i = 0; i < matrix->values.size(); ++i) {
        output << "std::vector<double>{";
        for (size_t j = 0; j < matrix->values[i].size(); ++j) {
            output << doubleLiteral(matrix->values[i][j]);
            if (j < matrix->values[i].size() - 1) output << ", ";
        }
        output << "}";
        if (i < matrix->values.size() - 1) output << ", ";
    }
    output << "}";
}

void CodeGenerator::generateBinaryOp(BinaryOp* op) {
    // Generate helper function calls based on operator
    // We'll use a simpler approach: generate expressions and let C++ handle overloads
    // For matrix operations, we need to use our helper functions
    
    if (op->op == TokenType::PLUS) {
        // Use operator+ which will handle all cases via overloads
        emit("(");
        generateExpression(op->left.get());
        emit(" + ");
        generateExpression(op->right.get());
        emit(")");
    } else if (op->op == TokenType::MINUS) {
        // Use operator- which will handle all cases via overloads
        emit("(");
        generateExpression(op->left.get());
        emit(" - ");
        generateExpression(op->right.get());
        emit(")");
    } else if (op->op == TokenType::MULTIPLY) {
        // Use operator* which will handle matrix*matrix, scalar*matrix, matrix*scalar, scalar*scalar
        emit("(");
        generateExpression(op->left.get());
        emit(" * ");
        generateExpression(op->right.get());
        emit(")");
    } else if (op->op == TokenType::DIVIDE) {
        // Use operator/ which will handle matrix/scalar, scalar/scalar
        emit("(");
        generateExpression(op->left.get());
        emit(" / ");
        generateExpression(op->right.get());
        emit(")");
    }
}

void CodeGenerator::generateUnaryOp(UnaryOp* op) {
    if (op->op == TokenType::MINUS) {
        emit("(-1.0 * ");
        generateExpression(op->operand.get());
        emit(")");
    } else if (op->op == TokenType::TRANSPOSE) {
        emit("transpose(");
        generateExpression(op->operand.get());
        emit(")");
    }
}

std::string CodeGenerator::sanitizeIdentifier(const std::string& name) {
    // Prefix every user variable so names like "int", "main" or "mul" can't
    // collide with C++ keywords or the helper functions emitted above
    std::string result = "v_" + name;
    for (char& c : result) {
        if (!std::isalnum(c) && c != '_') {
            c = '_';
        }
    }
    return result;
}


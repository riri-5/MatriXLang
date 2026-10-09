#ifndef INTERPRETER_HPP
#define INTERPRETER_HPP

#include "../include/ast.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>

using Matrix = std::vector<std::vector<double>>;

// A runtime value is either a scalar or a matrix
struct Value {
    bool isScalar;
    double scalar;
    Matrix matrix;

    static Value fromScalar(double s) { return Value{true, s, {}}; }
    static Value fromMatrix(Matrix m) { return Value{false, 0.0, std::move(m)}; }
};

// Tree-walking interpreter: executes a semantically valid program directly
// from its AST. It is the backend used in the browser (the whole compiler is
// built to WebAssembly), while CodeGenerator is the C++ backend.
class Interpreter {
public:
    std::string run(Program* program);

private:
    std::unordered_map<std::string, Matrix> variables;
    std::ostringstream out;

    void execute(Statement* stmt);
    Value evaluate(Expression* expr);
    Value applyBinary(TokenType op, const Value& left, const Value& right);
    static Matrix toStored(const Value& v);
    void print(const Value& v);
};

#endif // INTERPRETER_HPP

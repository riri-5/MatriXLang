#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <vector>
#include <memory>
#include "../include/lexer.hpp"

// Forward declarations
struct ASTNode;
struct Program;
struct Statement;
struct Expression;
struct MatrixLiteral;
struct BinaryOp;
struct UnaryOp;
struct Identifier;
struct NumberLiteral;
struct PrintStatement;
struct AssignmentStatement;
struct MatrixDeclaration;
struct BuiltinCall;

// AST Node base
struct ASTNode {
    virtual ~ASTNode() = default;
};

// Program: root node
struct Program : ASTNode {
    std::vector<std::unique_ptr<Statement>> statements;
};

// Statements
struct Statement : ASTNode {};

struct MatrixDeclaration : Statement {
    std::string name;
    std::unique_ptr<Expression> value;
    size_t line;
    size_t column;
    
    MatrixDeclaration(const std::string& n, std::unique_ptr<Expression> v, size_t l, size_t c)
        : name(n), value(std::move(v)), line(l), column(c) {}
};

struct AssignmentStatement : Statement {
    std::string name;
    std::unique_ptr<Expression> value;
    size_t line;
    size_t column;
    
    AssignmentStatement(const std::string& n, std::unique_ptr<Expression> v, size_t l, size_t c)
        : name(n), value(std::move(v)), line(l), column(c) {}
};

struct PrintStatement : Statement {
    std::unique_ptr<Expression> expr;
    size_t line;
    size_t column;
    
    PrintStatement(std::unique_ptr<Expression> e, size_t l, size_t c)
        : expr(std::move(e)), line(l), column(c) {}
};

// Expressions
struct Expression : ASTNode {};

struct MatrixLiteral : Expression {
    std::vector<std::vector<double>> values;
    size_t rows;
    size_t cols;
    size_t line;
    size_t column;
    
    MatrixLiteral(size_t r, size_t c, size_t l, size_t col)
        : rows(r), cols(c), line(l), column(col) {
        values.resize(r, std::vector<double>(c, 0.0));
    }
};

struct NumberLiteral : Expression {
    double value;
    size_t line;
    size_t column;
    
    NumberLiteral(double v, size_t l, size_t c) : value(v), line(l), column(c) {}
};

struct Identifier : Expression {
    std::string name;
    size_t line;
    size_t column;
    
    Identifier(const std::string& n, size_t l, size_t c) : name(n), line(l), column(c) {}
};

struct BinaryOp : Expression {
    TokenType op;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
    size_t line;
    size_t column;
    
    BinaryOp(TokenType o, std::unique_ptr<Expression> l, std::unique_ptr<Expression> r, size_t ln, size_t col)
        : op(o), left(std::move(l)), right(std::move(r)), line(ln), column(col) {}
};

struct UnaryOp : Expression {
    TokenType op;
    std::unique_ptr<Expression> operand;
    size_t line;
    size_t column;
    
    UnaryOp(TokenType o, std::unique_ptr<Expression> opd, size_t ln, size_t col)
        : op(o), operand(std::move(opd)), line(ln), column(col) {}
};

// Built-in matrix constructors: identity(n), zeros(r, c), ones(r, c)
// Arguments must be positive integer literals so dimensions are known at compile time.
struct BuiltinCall : Expression {
    std::string name;
    std::vector<size_t> args;
    size_t line;
    size_t column;

    BuiltinCall(const std::string& n, std::vector<size_t> a, size_t l, size_t c)
        : name(n), args(std::move(a)), line(l), column(c) {}
};

#endif // AST_HPP


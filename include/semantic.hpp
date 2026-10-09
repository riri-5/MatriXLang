#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "../include/ast.hpp"
#include <string>
#include <unordered_map>
#include <vector>

enum class Type {
    MATRIX,
    SCALAR,
    UNKNOWN
};

struct MatrixType {
    size_t rows;
    size_t cols;
    
    MatrixType(size_t r, size_t c) : rows(r), cols(c) {}
    
    bool operator==(const MatrixType& other) const {
        return rows == other.rows && cols == other.cols;
    }
};

struct SymbolInfo {
    Type type;
    MatrixType matrixType;  // Only valid if type == MATRIX
    size_t line;
    size_t column;
    
    SymbolInfo() : type(Type::UNKNOWN), matrixType(0, 0), line(0), column(0) {}
    
    SymbolInfo(Type t, size_t r = 0, size_t c = 0, size_t l = 0, size_t col = 0)
        : type(t), matrixType(r, c), line(l), column(col) {}
};

struct SemanticResult {
    bool success;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    
    SemanticResult() : success(true) {}
    void addError(const std::string& error) {
        success = false;
        errors.push_back(error);
    }
    void addWarning(const std::string& warning) {
        warnings.push_back(warning);
    }
};

class SemanticAnalyzer {
public:
    SemanticResult analyze(std::unique_ptr<Program>& program);
    
private:
    std::unordered_map<std::string, SymbolInfo> symbols;
    SemanticResult result;
    
    void analyzeStatement(Statement* stmt);
    Type analyzeExpression(Expression* expr, MatrixType& matrixType);
    void checkBinaryOp(BinaryOp* op, Type leftType, Type rightType, 
                      MatrixType& leftMatrix, MatrixType& rightMatrix,
                      Type& resultType, MatrixType& resultMatrix);
    void checkUnaryOp(UnaryOp* op, Type operandType, MatrixType& operandMatrix,
                     Type& resultType, MatrixType& resultMatrix);
};

#endif // SEMANTIC_HPP


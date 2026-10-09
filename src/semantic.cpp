#include "../include/semantic.hpp"
#include <sstream>

SemanticResult SemanticAnalyzer::analyze(std::unique_ptr<Program>& program) {
    symbols.clear();
    result = SemanticResult();
    
    for (auto& stmt : program->statements) {
        analyzeStatement(stmt.get());
    }
    
    return result;
}

void SemanticAnalyzer::analyzeStatement(Statement* stmt) {
    if (auto* decl = dynamic_cast<MatrixDeclaration*>(stmt)) {
        auto existing = symbols.find(decl->name);
        if (existing != symbols.end()) {
            std::stringstream ss;
            ss << "Line " << decl->line << ": Variable '" << decl->name
               << "' is already declared (line " << existing->second.line
               << "). Use '" << decl->name << " = ...;' to reassign it";
            result.addError(ss.str());
            return;
        }
        
        MatrixType matrixType(0, 0);
        Type exprType = analyzeExpression(decl->value.get(), matrixType);
        
        if (exprType == Type::UNKNOWN) {
            // An error was already reported inside the expression. Remember the name with an
            // unknown type so later uses don't add a misleading "undefined variable" error.
            symbols[decl->name] = SymbolInfo(Type::UNKNOWN, 0, 0, decl->line, decl->column);
            return;
        }
        
        if (exprType == Type::MATRIX) {
            symbols[decl->name] = SymbolInfo(Type::MATRIX, matrixType.rows, 
                                            matrixType.cols, decl->line, decl->column);
        } else {
            // Scalar assigned to matrix - treat as 1x1 matrix
            symbols[decl->name] = SymbolInfo(Type::MATRIX, 1, 1, decl->line, decl->column);
        }
        
    } else if (auto* assign = dynamic_cast<AssignmentStatement*>(stmt)) {
        if (symbols.find(assign->name) == symbols.end()) {
            std::stringstream ss;
            ss << "Line " << assign->line << ": Undefined variable '" << assign->name << "'";
            result.addError(ss.str());
            return;
        }
        
        MatrixType matrixType(0, 0);
        Type exprType = analyzeExpression(assign->value.get(), matrixType);
        
        if (exprType == Type::UNKNOWN) {
            // the specific error was already reported inside the expression
            return;
        }
        
        // A scalar stored in a matrix variable becomes a 1x1 matrix
        if (exprType == Type::SCALAR) {
            matrixType = MatrixType(1, 1);
            exprType = Type::MATRIX;
        }
        
        SymbolInfo& varInfo = symbols[assign->name];
        if (varInfo.type == Type::MATRIX && exprType == Type::MATRIX) {
            if (varInfo.matrixType.rows != matrixType.rows || 
                varInfo.matrixType.cols != matrixType.cols) {
                std::stringstream ss;
                ss << "Line " << assign->line << ": Dimension mismatch. Variable '" 
                   << assign->name << "' is " << varInfo.matrixType.rows << "x" 
                   << varInfo.matrixType.cols << ", but expression is " 
                   << matrixType.rows << "x" << matrixType.cols;
                result.addError(ss.str());
            }
        }
        
    } else if (auto* print = dynamic_cast<PrintStatement*>(stmt)) {
        // Any problem in the expression is reported while analyzing it
        MatrixType matrixType(0, 0);
        analyzeExpression(print->expr.get(), matrixType);
    }
}

Type SemanticAnalyzer::analyzeExpression(Expression* expr, MatrixType& matrixType) {
    if (dynamic_cast<NumberLiteral*>(expr)) {
        matrixType = MatrixType(0, 0);
        return Type::SCALAR;
    }
    
    if (auto* id = dynamic_cast<Identifier*>(expr)) {
        if (symbols.find(id->name) == symbols.end()) {
            std::stringstream ss;
            ss << "Line " << id->line << ": Undefined variable '" << id->name << "'";
            result.addError(ss.str());
            return Type::UNKNOWN;
        }
        
        SymbolInfo& info = symbols[id->name];
        if (info.type == Type::UNKNOWN) {
            return Type::UNKNOWN;  // its declaration already failed with an error
        }
        if (info.type == Type::MATRIX) {
            matrixType = info.matrixType;
            return Type::MATRIX;
        } else {
            matrixType = MatrixType(0, 0);
            return Type::SCALAR;
        }
    }
    
    if (auto* matrix = dynamic_cast<MatrixLiteral*>(expr)) {
        matrixType = MatrixType(matrix->rows, matrix->cols);
        return Type::MATRIX;
    }
    
    if (auto* call = dynamic_cast<BuiltinCall*>(expr)) {
        if (call->name == "identity") {
            matrixType = MatrixType(call->args[0], call->args[0]);
        } else {
            matrixType = MatrixType(call->args[0], call->args[1]);
        }
        return Type::MATRIX;
    }
    
    if (auto* binOp = dynamic_cast<BinaryOp*>(expr)) {
        MatrixType leftMatrix(0, 0), rightMatrix(0, 0);
        Type leftType = analyzeExpression(binOp->left.get(), leftMatrix);
        Type rightType = analyzeExpression(binOp->right.get(), rightMatrix);
        
        if (leftType == Type::UNKNOWN || rightType == Type::UNKNOWN) {
            return Type::UNKNOWN;
        }
        
        Type resultType;
        checkBinaryOp(binOp, leftType, rightType, leftMatrix, rightMatrix, 
                     resultType, matrixType);
        return resultType;
    }
    
    if (auto* unOp = dynamic_cast<UnaryOp*>(expr)) {
        MatrixType operandMatrix(0, 0);
        Type operandType = analyzeExpression(unOp->operand.get(), operandMatrix);
        
        if (operandType == Type::UNKNOWN) {
            return Type::UNKNOWN;
        }
        
        Type resultType;
        checkUnaryOp(unOp, operandType, operandMatrix, resultType, matrixType);
        return resultType;
    }
    
    return Type::UNKNOWN;
}

void SemanticAnalyzer::checkBinaryOp(BinaryOp* op, Type leftType, Type rightType,
                                     MatrixType& leftMatrix, MatrixType& rightMatrix,
                                     Type& resultType, MatrixType& resultMatrix) {
    if (op->op == TokenType::PLUS || op->op == TokenType::MINUS) {
        // Element-wise operations
        if (leftType == Type::SCALAR && rightType == Type::SCALAR) {
            resultType = Type::SCALAR;
            resultMatrix = MatrixType(0, 0);
        } else if (leftType == Type::MATRIX && rightType == Type::SCALAR) {
            resultType = Type::MATRIX;
            resultMatrix = leftMatrix;
        } else if (leftType == Type::SCALAR && rightType == Type::MATRIX) {
            resultType = Type::MATRIX;
            resultMatrix = rightMatrix;
        } else if (leftType == Type::MATRIX && rightType == Type::MATRIX) {
            if (leftMatrix.rows != rightMatrix.rows || leftMatrix.cols != rightMatrix.cols) {
                std::stringstream ss;
                ss << "Line " << op->line << ": Dimension mismatch for " 
                   << (op->op == TokenType::PLUS ? "addition" : "subtraction")
                   << ". Left: " << leftMatrix.rows << "x" << leftMatrix.cols
                   << ", Right: " << rightMatrix.rows << "x" << rightMatrix.cols;
                result.addError(ss.str());
                resultType = Type::UNKNOWN;
            } else {
                resultType = Type::MATRIX;
                resultMatrix = leftMatrix;
            }
        }
    } else if (op->op == TokenType::MULTIPLY) {
        if (leftType == Type::SCALAR && rightType == Type::SCALAR) {
            resultType = Type::SCALAR;
            resultMatrix = MatrixType(0, 0);
        } else if (leftType == Type::SCALAR && rightType == Type::MATRIX) {
            resultType = Type::MATRIX;
            resultMatrix = rightMatrix;
        } else if (leftType == Type::MATRIX && rightType == Type::SCALAR) {
            resultType = Type::MATRIX;
            resultMatrix = leftMatrix;
        } else if (leftType == Type::MATRIX && rightType == Type::MATRIX) {
            if (leftMatrix.cols != rightMatrix.rows) {
                std::stringstream ss;
                ss << "Line " << op->line << ": Matrix multiplication dimension mismatch. "
                   << "Left: " << leftMatrix.rows << "x" << leftMatrix.cols
                   << ", Right: " << rightMatrix.rows << "x" << rightMatrix.cols
                   << " (left cols must equal right rows)";
                result.addError(ss.str());
                resultType = Type::UNKNOWN;
            } else {
                resultType = Type::MATRIX;
                resultMatrix = MatrixType(leftMatrix.rows, rightMatrix.cols);
            }
        }
    } else if (op->op == TokenType::DIVIDE) {
        if (leftType == Type::SCALAR && rightType == Type::SCALAR) {
            resultType = Type::SCALAR;
            resultMatrix = MatrixType(0, 0);
        } else if (leftType == Type::MATRIX && rightType == Type::SCALAR) {
            resultType = Type::MATRIX;
            resultMatrix = leftMatrix;
        } else if (leftType == Type::SCALAR && rightType == Type::MATRIX) {
            std::stringstream ss;
            ss << "Line " << op->line << ": Cannot divide scalar by matrix";
            result.addError(ss.str());
            resultType = Type::UNKNOWN;
        } else if (leftType == Type::MATRIX && rightType == Type::MATRIX) {
            std::stringstream ss;
            ss << "Line " << op->line << ": Matrix division not supported (only scalar division)";
            result.addError(ss.str());
            resultType = Type::UNKNOWN;
        }
    }
}

void SemanticAnalyzer::checkUnaryOp(UnaryOp* op, Type operandType, MatrixType& operandMatrix,
                                    Type& resultType, MatrixType& resultMatrix) {
    if (op->op == TokenType::MINUS) {
        // Negation keeps the operand's type and shape
        resultType = operandType;
        resultMatrix = operandMatrix;
    } else if (op->op == TokenType::TRANSPOSE) {
        if (operandType == Type::SCALAR) {
            std::stringstream ss;
            ss << "Line " << op->line << ": Cannot transpose scalar";
            result.addError(ss.str());
            resultType = Type::UNKNOWN;
        } else {
            resultType = Type::MATRIX;
            resultMatrix = MatrixType(operandMatrix.cols, operandMatrix.rows);
        }
    }
}


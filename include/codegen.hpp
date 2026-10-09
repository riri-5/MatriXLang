#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "../include/ast.hpp"
#include <string>
#include <ostream>
#include <sstream>

class CodeGenerator {
public:
    std::string generate(std::unique_ptr<Program>& program);
    
private:
    std::stringstream output;
    int indentLevel;
    
    void emit(const std::string& code);
    void emitLine(const std::string& code = "");
    void increaseIndent();
    void decreaseIndent();
    std::string getIndent();
    
    void generateProgram(Program* program);
    void generateStatement(Statement* stmt);
    void generateMatrixDeclaration(MatrixDeclaration* decl);
    void generateAssignment(AssignmentStatement* assign);
    void generatePrint(PrintStatement* print);
    void generateExpression(Expression* expr);
    void generateMatrixLiteral(MatrixLiteral* matrix);
    void generateBinaryOp(BinaryOp* op);
    void generateUnaryOp(UnaryOp* op);
    void generateBuiltinCall(BuiltinCall* call);
    void generateStoredValue(Expression* expr);
    bool isScalarExpression(Expression* expr);
    
    std::string sanitizeIdentifier(const std::string& name);
};

#endif // CODEGEN_HPP


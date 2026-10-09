#ifndef PARSER_HPP
#define PARSER_HPP

#include "../include/lexer.hpp"
#include "../include/ast.hpp"
#include <vector>
#include <memory>
#include <string>

class Parser {
public:
    Parser(const std::vector<Token>& tokens);
    
    std::unique_ptr<Program> parse();
    
private:
    std::vector<Token> tokens;
    size_t pos;
    
    Token current() const;
    Token peek(size_t offset = 0) const;
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& expected);
    bool isAtEnd() const;
    
    std::unique_ptr<Program> parseProgram();
    std::unique_ptr<Statement> parseStatement();
    std::unique_ptr<Statement> parseMatrixDeclaration();
    std::unique_ptr<Statement> parseAssignment();
    std::unique_ptr<Statement> parsePrint();
    std::unique_ptr<Expression> parseExpression();
    std::unique_ptr<Expression> parseTerm();
    std::unique_ptr<Expression> parseFactor();
    std::unique_ptr<MatrixLiteral> parseMatrixLiteral();
    std::unique_ptr<Expression> parseBuiltinCall(const Token& name);
    double parseSignedNumber();
};

#endif // PARSER_HPP


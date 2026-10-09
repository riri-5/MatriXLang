#include "../include/parser.hpp"
#include <sstream>
#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {}

Token Parser::current() const {
    if (pos >= tokens.size()) {
        return Token(TokenType::END_OF_FILE, "", 0, 0);
    }
    return tokens[pos];
}

Token Parser::peek(size_t offset) const {
    size_t idx = pos + offset;
    if (idx >= tokens.size()) {
        return Token(TokenType::END_OF_FILE, "", 0, 0);
    }
    return tokens[idx];
}

bool Parser::match(TokenType type) {
    if (current().type == type) {
        pos++;
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& expected) {
    if (current().type == type) {
        Token token = current();
        pos++;
        return token;
    }
    
    std::stringstream ss;
    ss << "Expected " << expected << " at line " << current().line 
       << ", column " << current().column;
    throw std::runtime_error(ss.str());
}

bool Parser::isAtEnd() const {
    return pos >= tokens.size() || current().type == TokenType::END_OF_FILE;
}

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();
    
    while (!isAtEnd()) {
        if (current().type == TokenType::ERROR) {
            std::stringstream ss;
            ss << "Lexer error: " << current().value;
            throw std::runtime_error(ss.str());
        }
        
        try {
            auto stmt = parseStatement();
            if (stmt) {
                program->statements.push_back(std::move(stmt));
            }
        } catch (const std::exception& e) {
            throw;
        }
    }
    
    return program;
}

std::unique_ptr<Statement> Parser::parseStatement() {
    if (match(TokenType::MATRIX)) {
        return parseMatrixDeclaration();
    } else if (match(TokenType::PRINT)) {
        return parsePrint();
    } else if (current().type == TokenType::IDENTIFIER) {
        return parseAssignment();
    }
    
    std::stringstream ss;
    ss << "Unexpected token at line " << current().line << ", column " << current().column;
    throw std::runtime_error(ss.str());
}

std::unique_ptr<Statement> Parser::parseMatrixDeclaration() {
    Token id = consume(TokenType::IDENTIFIER, "identifier");
    consume(TokenType::ASSIGN, "=");
    auto expr = parseExpression();
    consume(TokenType::SEMICOLON, ";");
    
    return std::make_unique<MatrixDeclaration>(
        id.value, std::move(expr), id.line, id.column
    );
}

std::unique_ptr<Statement> Parser::parseAssignment() {
    Token id = current();
    consume(TokenType::IDENTIFIER, "identifier");
    consume(TokenType::ASSIGN, "=");
    auto expr = parseExpression();
    consume(TokenType::SEMICOLON, ";");
    
    return std::make_unique<AssignmentStatement>(
        id.value, std::move(expr), id.line, id.column
    );
}

std::unique_ptr<Statement> Parser::parsePrint() {
    size_t line = current().line;
    size_t col = current().column;
    consume(TokenType::LPAREN, "(");
    auto expr = parseExpression();
    consume(TokenType::RPAREN, ")");
    consume(TokenType::SEMICOLON, ";");
    
    return std::make_unique<PrintStatement>(std::move(expr), line, col);
}

std::unique_ptr<Expression> Parser::parseExpression() {
    auto left = parseTerm();
    
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        Token opToken = peek(-1);
        auto right = parseTerm();
        left = std::make_unique<BinaryOp>(opToken.type, std::move(left), std::move(right),
                                          opToken.line, opToken.column);
    }
    
    return left;
}

std::unique_ptr<Expression> Parser::parseTerm() {
    auto left = parseFactor();
    
    while (match(TokenType::MULTIPLY) || match(TokenType::DIVIDE)) {
        Token opToken = peek(-1);
        auto right = parseFactor();
        left = std::make_unique<BinaryOp>(opToken.type, std::move(left), std::move(right),
                                          opToken.line, opToken.column);
    }
    
    return left;
}

std::unique_ptr<Expression> Parser::parseFactor() {
    if (match(TokenType::NUMBER)) {
        Token token = peek(-1);
        double value = std::stod(token.value);
        return std::make_unique<NumberLiteral>(value, token.line, token.column);
    }
    
    // Unary minus: -A, -(A + B), -3
    if (match(TokenType::MINUS)) {
        Token minus = peek(-1);
        auto operand = parseFactor();
        return std::make_unique<UnaryOp>(TokenType::MINUS, std::move(operand), minus.line, minus.column);
    }
    
    if (match(TokenType::IDENTIFIER)) {
        Token token = peek(-1);
        if (current().type == TokenType::LPAREN) {
            return parseBuiltinCall(token);
        }
        return std::make_unique<Identifier>(token.value, token.line, token.column);
    }
    
    if (match(TokenType::TRANSPOSE)) {
        size_t line = peek(-1).line;
        size_t col = peek(-1).column;
        consume(TokenType::LPAREN, "(");
        auto expr = parseExpression();
        consume(TokenType::RPAREN, ")");
        return std::make_unique<UnaryOp>(TokenType::TRANSPOSE, std::move(expr), line, col);
    }
    
    if (match(TokenType::LBRACKET)) {
        return parseMatrixLiteral();
    }
    
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        consume(TokenType::RPAREN, ")");
        return expr;
    }
    
    std::stringstream ss;
    ss << "Unexpected token in factor at line " << current().line 
       << ", column " << current().column;
    throw std::runtime_error(ss.str());
}

double Parser::parseSignedNumber() {
    bool negative = match(TokenType::MINUS);
    double value = std::stod(consume(TokenType::NUMBER, "number").value);
    return negative ? -value : value;
}

std::unique_ptr<Expression> Parser::parseBuiltinCall(const Token& name) {
    size_t expectedArgs;
    if (name.value == "identity") {
        expectedArgs = 1;
    } else if (name.value == "zeros" || name.value == "ones") {
        expectedArgs = 2;
    } else {
        std::stringstream ss;
        ss << "Unknown function '" << name.value << "' at line " << name.line
           << ", column " << name.column << " (available: identity, zeros, ones)";
        throw std::runtime_error(ss.str());
    }
    
    consume(TokenType::LPAREN, "(");
    std::vector<size_t> args;
    do {
        Token arg = consume(TokenType::NUMBER, "a positive whole number");
        double value = std::stod(arg.value);
        if (value < 1 || value != static_cast<double>(static_cast<size_t>(value)) || value > 4096) {
            std::stringstream ss;
            ss << "Argument to '" << name.value << "' must be a whole number between 1 and 4096 at line "
               << arg.line << ", column " << arg.column;
            throw std::runtime_error(ss.str());
        }
        args.push_back(static_cast<size_t>(value));
    } while (match(TokenType::COMMA));
    consume(TokenType::RPAREN, ")");
    
    if (args.size() != expectedArgs) {
        std::stringstream ss;
        ss << "'" << name.value << "' takes " << expectedArgs << " argument"
           << (expectedArgs == 1 ? "" : "s") << " but got " << args.size()
           << " at line " << name.line << ", column " << name.column;
        throw std::runtime_error(ss.str());
    }
    
    return std::make_unique<BuiltinCall>(name.value, std::move(args), name.line, name.column);
}

std::unique_ptr<MatrixLiteral> Parser::parseMatrixLiteral() {
    size_t line = peek(-1).line;
    size_t col = peek(-1).column;
    
    std::vector<std::vector<double>> rows;
    
    // Parse first row
    if (current().type == TokenType::LBRACKET) {
        consume(TokenType::LBRACKET, "[");
        std::vector<double> row;
        
        if (current().type == TokenType::NUMBER || current().type == TokenType::MINUS) {
            row.push_back(parseSignedNumber());
            while (match(TokenType::COMMA)) {
                row.push_back(parseSignedNumber());
            }
        }
        
        consume(TokenType::RBRACKET, "]");
        rows.push_back(row);
    }
    
    // Parse remaining rows
    while (match(TokenType::COMMA) && current().type == TokenType::LBRACKET) {
        consume(TokenType::LBRACKET, "[");
        std::vector<double> row;
        
        if (current().type == TokenType::NUMBER || current().type == TokenType::MINUS) {
            row.push_back(parseSignedNumber());
            while (match(TokenType::COMMA)) {
                row.push_back(parseSignedNumber());
            }
        }
        
        consume(TokenType::RBRACKET, "]");
        rows.push_back(row);
    }
    
    consume(TokenType::RBRACKET, "]");
    
    if (rows.empty() || rows[0].empty()) {
        std::stringstream ss;
        ss << "Empty matrix literal at line " << line << ", column " << col;
        throw std::runtime_error(ss.str());
    }
    
    size_t numRows = rows.size();
    size_t numCols = rows[0].size();
    
    // Validate all rows have same length
    for (const auto& row : rows) {
        if (row.size() != numCols) {
            std::stringstream ss;
            ss << "Matrix rows must have the same length at line " << line << ", column " << col;
            throw std::runtime_error(ss.str());
        }
    }
    
    auto matrix = std::make_unique<MatrixLiteral>(numRows, numCols, line, col);
    matrix->values = rows;
    
    return matrix;
}


#include "../include/lexer.hpp"
#include <sstream>
#include <iomanip>

Lexer::Lexer(const std::string& source) 
    : source(source), pos(0), line(1), column(1) {}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    Token token = nextToken();
    while (token.type != TokenType::END_OF_FILE && token.type != TokenType::ERROR) {
        tokens.push_back(token);
        token = nextToken();
    }
    if (token.type == TokenType::ERROR) {
        tokens.push_back(token);
    }
    return tokens;
}

char Lexer::current() const {
    if (pos >= source.length()) return '\0';
    return source[pos];
}

char Lexer::peek(size_t offset) const {
    if (pos + offset >= source.length()) return '\0';
    return source[pos + offset];
}

void Lexer::advance() {
    if (pos < source.length()) {
        if (source[pos] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
        pos++;
    }
}

void Lexer::skipWhitespace() {
    while (pos < source.length() && std::isspace(current())) {
        advance();
    }
}

void Lexer::skipComment() {
    if (current() == '/' && peek() == '/') {
        while (pos < source.length() && current() != '\n') {
            advance();
        }
    }
}

Token Lexer::makeToken(TokenType type, const std::string& value) {
    size_t l = line;
    size_t c = column;
    advance();
    return Token(type, value, l, c);
}

Token Lexer::scanNumber() {
    size_t start = pos;
    size_t startLine = line;
    size_t startCol = column;
    
    bool hasDot = false;
    while (pos < source.length() && (std::isdigit(current()) || current() == '.')) {
        if (current() == '.') {
            if (hasDot) break;
            hasDot = true;
        }
        advance();
    }

    // Optional exponent: 1e5, 2.5E-3, 4e+2
    if ((current() == 'e' || current() == 'E') &&
        (std::isdigit(peek()) || ((peek() == '+' || peek() == '-') && std::isdigit(peek(2))))) {
        advance();
        if (current() == '+' || current() == '-') advance();
        while (pos < source.length() && std::isdigit(current())) {
            advance();
        }
    }

    std::string value = source.substr(start, pos - start);
    return Token(TokenType::NUMBER, value, startLine, startCol);
}

Token Lexer::scanIdentifier() {
    size_t start = pos;
    size_t startLine = line;
    size_t startCol = column;
    
    while (pos < source.length() && (std::isalnum(current()) || current() == '_')) {
        advance();
    }
    
    std::string value = source.substr(start, pos - start);
    
    // Check for keywords
    if (value == "matrix") {
        return Token(TokenType::MATRIX, value, startLine, startCol);
    } else if (value == "print") {
        return Token(TokenType::PRINT, value, startLine, startCol);
    } else if (value == "transpose") {
        return Token(TokenType::TRANSPOSE, value, startLine, startCol);
    }
    
    return Token(TokenType::IDENTIFIER, value, startLine, startCol);
}

Token Lexer::nextToken() {
    while (pos < source.length()) {
        skipWhitespace();
        skipComment();
        
        if (pos >= source.length()) break;
        
        char c = current();
        
        // Skip newlines (they're already handled by skipWhitespace, but be explicit)
        if (c == '\n' || c == '\r') {
            advance();
            continue;
        }
        
        // Single character tokens
        if (c == ';') return makeToken(TokenType::SEMICOLON, ";");
        if (c == ',') return makeToken(TokenType::COMMA, ",");
        if (c == '(') return makeToken(TokenType::LPAREN, "(");
        if (c == ')') return makeToken(TokenType::RPAREN, ")");
        if (c == '[') return makeToken(TokenType::LBRACKET, "[");
        if (c == ']') return makeToken(TokenType::RBRACKET, "]");
        if (c == '=') return makeToken(TokenType::ASSIGN, "=");
        if (c == '+') return makeToken(TokenType::PLUS, "+");
        if (c == '-') return makeToken(TokenType::MINUS, "-");
        if (c == '*') return makeToken(TokenType::MULTIPLY, "*");
        if (c == '/') return makeToken(TokenType::DIVIDE, "/");
        
        // Numbers
        if (std::isdigit(c) || c == '.') {
            return scanNumber();
        }
        
        // Identifiers
        if (std::isalpha(c) || c == '_') {
            return scanIdentifier();
        }
        
        // Unknown character
        std::string error = "Unexpected character: ";
        error += c;
        return Token(TokenType::ERROR, error, line, column);
    }
    
    return Token(TokenType::END_OF_FILE, "", line, column);
}


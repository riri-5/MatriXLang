#ifndef LEXER_HPP
#define LEXER_HPP

#include <string>
#include <vector>
#include <cctype>

enum class TokenType {
    // Keywords
    MATRIX, PRINT, TRANSPOSE,
    
    // Identifiers and literals
    IDENTIFIER, NUMBER,
    
    // Operators
    PLUS, MINUS, MULTIPLY, DIVIDE, ASSIGN,
    
    // Punctuation
    SEMICOLON, COMMA, LBRACKET, RBRACKET, LPAREN, RPAREN,
    
    // Special
    END_OF_FILE, ERROR
};

struct Token {
    TokenType type;
    std::string value;
    size_t line;
    size_t column;
    
    Token(TokenType t, const std::string& v, size_t l, size_t c)
        : type(t), value(v), line(l), column(c) {}
};

class Lexer {
public:
    Lexer(const std::string& source);
    
    std::vector<Token> tokenize();
    Token nextToken();
    
private:
    std::string source;
    size_t pos;
    size_t line;
    size_t column;
    
    char current() const;
    char peek(size_t offset = 1) const;
    void advance();
    void skipWhitespace();
    void skipComment();
    
    Token makeToken(TokenType type, const std::string& value);
    Token scanNumber();
    Token scanIdentifier();
    Token scanString();
};

#endif // LEXER_HPP


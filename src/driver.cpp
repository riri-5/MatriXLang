#include "../include/driver.hpp"
#include "../include/lexer.hpp"
#include "../include/parser.hpp"
#include "../include/semantic.hpp"
#include "../include/codegen.hpp"
#include "../include/interpreter.hpp"
#include <sstream>

static std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::MATRIX: return "MATRIX";
        case TokenType::PRINT: return "PRINT";
        case TokenType::TRANSPOSE: return "TRANSPOSE";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::MULTIPLY: return "MULTIPLY";
        case TokenType::DIVIDE: return "DIVIDE";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

static std::string operatorSymbol(TokenType type) {
    switch (type) {
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::MULTIPLY: return "*";
        case TokenType::DIVIDE: return "/";
        default: return "?";
    }
}

static void printExpression(std::ostream& os, Expression* expr, int indent) {
    std::string pad(indent * 2, ' ');
    if (auto* num = dynamic_cast<NumberLiteral*>(expr)) {
        os << pad << "Number " << num->value << "\n";
    } else if (auto* id = dynamic_cast<Identifier*>(expr)) {
        os << pad << "Identifier " << id->name << "\n";
    } else if (auto* literal = dynamic_cast<MatrixLiteral*>(expr)) {
        os << pad << "MatrixLiteral " << literal->rows << "x" << literal->cols << "\n";
    } else if (auto* call = dynamic_cast<BuiltinCall*>(expr)) {
        os << pad << "Call " << call->name << "(";
        for (size_t i = 0; i < call->args.size(); ++i) {
            os << (i ? ", " : "") << call->args[i];
        }
        os << ")\n";
    } else if (auto* binOp = dynamic_cast<BinaryOp*>(expr)) {
        os << pad << "BinaryOp " << operatorSymbol(binOp->op) << "\n";
        printExpression(os, binOp->left.get(), indent + 1);
        printExpression(os, binOp->right.get(), indent + 1);
    } else if (auto* unOp = dynamic_cast<UnaryOp*>(expr)) {
        os << pad << (unOp->op == TokenType::TRANSPOSE ? "Transpose" : "Negate") << "\n";
        printExpression(os, unOp->operand.get(), indent + 1);
    }
}

static std::string astToString(Program* program) {
    std::ostringstream os;
    os << "Program\n";
    for (const auto& stmt : program->statements) {
        if (auto* decl = dynamic_cast<MatrixDeclaration*>(stmt.get())) {
            os << "  MatrixDeclaration " << decl->name << "\n";
            printExpression(os, decl->value.get(), 2);
        } else if (auto* assign = dynamic_cast<AssignmentStatement*>(stmt.get())) {
            os << "  Assignment " << assign->name << "\n";
            printExpression(os, assign->value.get(), 2);
        } else if (auto* print = dynamic_cast<PrintStatement*>(stmt.get())) {
            os << "  Print\n";
            printExpression(os, print->expr.get(), 2);
        }
    }
    return os.str();
}

CompileResult compileSource(const std::string& source, Mode mode) {
    CompileResult result{false, "", ""};

    // Lexing
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    if (mode == Mode::TOKENS) {
        std::ostringstream os;
        for (const auto& token : tokens) {
            os << tokenTypeName(token.type) << "(" << token.value << ") ["
               << token.line << ":" << token.column << "]\n";
        }
        result.output = os.str();
    }

    for (const auto& token : tokens) {
        if (token.type == TokenType::ERROR) {
            std::ostringstream os;
            os << "Lexer error: " << token.value << " at line " << token.line
               << ", column " << token.column << "\n";
            result.errors = os.str();
            return result;
        }
    }
    if (mode == Mode::TOKENS) {
        result.ok = true;
        return result;
    }

    // Parsing
    std::unique_ptr<Program> program;
    try {
        Parser parser(tokens);
        program = parser.parse();
    } catch (const std::exception& e) {
        result.errors = std::string("Parser error: ") + e.what() + "\n";
        return result;
    }

    if (mode == Mode::AST) {
        result.ok = true;
        result.output = astToString(program.get());
        return result;
    }

    // Semantic analysis
    SemanticAnalyzer analyzer;
    SemanticResult semantic = analyzer.analyze(program);
    if (!semantic.success) {
        for (const auto& error : semantic.errors) {
            result.errors += "Semantic error: " + error + "\n";
        }
        return result;
    }

    // Backends
    result.ok = true;
    if (mode == Mode::ANALYZE) {
        result.output = "Semantic analysis: SUCCESS\n";
        for (const auto& warning : semantic.warnings) {
            result.output += "Warning: " + warning + "\n";
        }
    } else if (mode == Mode::CPP) {
        CodeGenerator codegen;
        result.output = codegen.generate(program);
    } else {
        Interpreter interpreter;
        result.output = interpreter.run(program.get());
    }
    return result;
}

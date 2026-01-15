#include "parser.h"
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cctype>

// ---------- Utilitários ----------
char CPLParser::peek() const {
    return pos < input.size() ? input[pos] : '\0';
}

char CPLParser::get() {
    return pos < input.size() ? input[pos++] : '\0';
}

void CPLParser::skipWhitespace() {
    while (std::isspace(peek())) pos++;
}

bool CPLParser::match(const std::string& expected) {
    skipWhitespace();
    if (input.substr(pos, expected.size()) == expected) {
        pos += expected.size();
        return true;
    }
    return false;
}

// ---------- Parser de CPL ----------

CPLParser::CPLParser(const std::string& input_) : input(input_), pos(0) {}

CPLFormula CPLParser::parse() {
    skipWhitespace();
    CPLFormula result = parseFormula();
    skipWhitespace();
    if (pos != input.size()) throw std::runtime_error("Extra entry after CPL formula.");
    return result;
}

CPLFormula CPLParser::parseFormula() {
    return parseBinary(0);
}

// ---------- Precedência dos conectivos ----------
int precedence(CPLConnective op) {
    switch (op) {
        case CPLConnective::IFF:     return 1;
        case CPLConnective::IMPLIES: return 2;
        case CPLConnective::OR:      return 3;
        case CPLConnective::AND:     return 4;
        case CPLConnective::NOT:     return 5;
        default: return 0;
    }
}

// ---------- Parser de expressões ----------
CPLFormula CPLParser::parseBinary(int minPrec) {
    CPLFormula left = parseUnary();
    while (true) {
        skipWhitespace();
        CPLConnective op;
        if (match("∧")) op = CPLConnective::AND;
        else if (match("∨")) op = CPLConnective::OR;
        else if (match("→")) op = CPLConnective::IMPLIES;
        else if (match("↔")) op = CPLConnective::IFF;
        else break;

        int prec = precedence(op);
        if (prec < minPrec) break;

        CPLFormula right = parseBinary(prec + 1);
        left = CPLFormula::binary(op, std::move(left), std::move(right));
    }
    return left;
}

CPLFormula CPLParser::parseUnary() {
    skipWhitespace();
    if (match("¬")) {
        return CPLFormula::unary(CPLConnective::NOT, parseUnary());
    } else {
        return parsePrimary();
    }
}

CPLFormula CPLParser::parsePrimary() {
    skipWhitespace();
    if (match("(")) {
        CPLFormula f = parseFormula();
        if (!match(")")) throw std::runtime_error("Right parenthesis expected.");
        return f;
    }

    if (std::isalpha(peek()) || peek() == '_') {
        std::string name;
        while (std::isalnum(peek()) || peek() == '_')
            name += get();
        return CPLFormula::variable(name);
    }

    throw std::runtime_error("Unexpected symbol in CPL formula.");
}

// ---------- ModalParser utilitários ----------

char ModalParser::peek() const {
    return pos < input.size() ? input[pos] : '\0';
}

char ModalParser::get() {
    return pos < input.size() ? input[pos++] : '\0';
}

void ModalParser::skipWhitespace() {
    while (std::isspace(peek())) pos++;
}

bool ModalParser::match(const std::string& expected) {
    skipWhitespace();
    if (input.substr(pos, expected.size()) == expected) {
        pos += expected.size();
        return true;
    }
    return false;
}

// ---------- Construtor ----------

ModalParser::ModalParser(const std::string& input_) : input(input_), pos(0) {}

ModalFormula ModalParser::parse() {
    skipWhitespace();
    ModalFormula result = parseFormula();
    skipWhitespace();
    if (pos != input.size()) throw std::runtime_error("Extra entry after modal formula.");
    return result;
}

// ---------- Precedência dos conectivos Ł ----------

int modalPrecedence(ModalConnective op) {
    switch (op) {
        case ModalConnective::IFF:     return 1;
        case ModalConnective::IMPLIES: return 2;
        case ModalConnective::OR:      return 3;
        case ModalConnective::AND:     return 4;
        case ModalConnective::ODOT:    return 5;
        case ModalConnective::OPLUS:   return 6;
        case ModalConnective::NOT:     return 7;
        default: return 0;
    }
}

// ---------- Parser principal ----------

ModalFormula ModalParser::parseFormula() {
    return parseBinary(0);
}

ModalFormula ModalParser::parseBinary(int minPrec) {
    ModalFormula left = parseUnary();
    while (true) {
        skipWhitespace();
        ModalConnective op;

        if (match("↔")) op = ModalConnective::IFF;
        else if (match("→")) op = ModalConnective::IMPLIES;
        else if (match("∨")) op = ModalConnective::OR;
        else if (match("∧")) op = ModalConnective::AND;
        else if (match("⊙")) op = ModalConnective::ODOT;
        else if (match("⊕")) op = ModalConnective::OPLUS;
        else break;

        int prec = modalPrecedence(op);
        if (prec < minPrec) break;

        ModalFormula right = parseBinary(prec + 1);
        left = ModalFormula::binary(op, std::move(left), std::move(right));
    }
    return left;
}

ModalFormula ModalParser::parseUnary() {
    skipWhitespace();
    if (match("¬")) {
        return ModalFormula::unary(ModalConnective::NOT, parseUnary());
    } else {
        return parsePrimary();
    }
}

ModalFormula ModalParser::parsePrimary() {
    skipWhitespace();

    if (match("(")) {
        ModalFormula f = parseFormula();
        if (!match(")")) throw std::runtime_error("Right parenthesis expected.");
        return f;
    }

    if (match("P")) {
        if (!match("(")) throw std::runtime_error("Expected '(' after P.");
        CPLFormula inner = parseCPLInsideP();
        return ModalFormula::patom(std::move(inner));
    }

    throw std::runtime_error("Unexpected symbol in CPL formula.");
}

CPLFormula ModalParser::parseCPLInsideP() {
    std::string buffer;
    int parenDepth = 1;

    while (pos < input.size() && parenDepth > 0) {
        char c = get();

        if (c == '(') {
            parenDepth++;
            buffer += c;
        }
        else if (c == ')') {
            parenDepth--;
            if (parenDepth > 0) buffer += c;
        }
        else {
            buffer += c;
        }
    }

    CPLParser subparser(buffer);
    return subparser.parse();
}

// ---------- Carregar fórmulas modais de um arquivo ----------
std::vector<ModalFormula> loadModalFormulasFromFile(const std::string& filename) {
    std::ifstream infile(filename);
    if (!infile) throw std::runtime_error("Could not open file: " + filename);

    std::vector<ModalFormula> formulas;
    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty()) continue;
        ModalParser parser(line);
        formulas.push_back(parser.parse());
    }
    return formulas;
}


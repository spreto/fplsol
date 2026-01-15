#ifndef PARSER_HPP
#define PARSER_HPP

#include "formula.h"
#include <string>
#include <vector>
#include <memory>

class CPLParser {
public:
    explicit CPLParser(const std::string& input);
    CPLFormula parse();

private:
    std::string input;
    size_t pos;

    char peek() const;
    char get();
    void skipWhitespace();
    bool match(const std::string& expected);

    CPLFormula parseFormula();
    CPLFormula parsePrimary();
    CPLFormula parseUnary();
    CPLFormula parseBinary(int minPrecedence);
};

class ModalParser {
public:
    explicit ModalParser(const std::string& input);
    ModalFormula parse();

private:
    std::string input;
    size_t pos;

    char peek() const;
    char get();
    void skipWhitespace();
    bool match(const std::string& expected);

    ModalFormula parseFormula();
    ModalFormula parsePrimary();
    ModalFormula parseUnary();
    ModalFormula parseBinary(int minPrecedence);

    CPLFormula parseCPLInsideP(); // usado para ler fórmulas CPL dentro de P(...)
};

// Utilitário para carregar várias fórmulas modais de um arquivo
std::vector<ModalFormula> loadModalFormulasFromFile(const std::string& filename);

#endif // PARSER_HPP


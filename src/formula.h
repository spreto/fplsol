#ifndef FORMULA_HPP
#define FORMULA_HPP

#include <memory>
#include <string>
#include <unordered_map>

// ----------- Nível 1: CPL -----------

enum class CPLConnective {
    VAR,
    NOT,
    AND,
    OR,
    IMPLIES,
    IFF
};

struct CPLFormula {
    mutable std::string cachedString; // cache de toString()
    mutable bool cached = false;

    CPLConnective op;
    std::string var; // só usado se op == VAR
    std::unique_ptr<CPLFormula> left;
    std::unique_ptr<CPLFormula> right;

    // Construtores estáticos auxiliares
    static CPLFormula variable(const std::string& name);
    static CPLFormula unary(CPLConnective op, CPLFormula operand);
    static CPLFormula binary(CPLConnective op, CPLFormula lhs, CPLFormula rhs);

    CPLFormula();
    CPLFormula(const CPLFormula&) = delete;
    CPLFormula(CPLFormula&&) = default;
    CPLFormula& operator=(const CPLFormula&) = delete;
    CPLFormula& operator=(CPLFormula&&) = default;

    void collectPropVars(std::unordered_map<std::string, int>& propVarToId) const;
    std::string toString() const;
    std::unique_ptr<CPLFormula> clone() const;
};

// ----------- Nível 2: FP(Ł) -----------

enum class ModalConnective {
    P_ATOM,     // Pφ
    NOT,
    AND,
    OR,
    IMPLIES,
    IFF,
    OPLUS,      // ⊕ disjunção forte
    ODOT        // ⊙ conjunção forte
};

struct ModalFormula {
    mutable std::string cachedString; // cache de toString()
    mutable bool cached = false;

    ModalConnective op;
    std::unique_ptr<CPLFormula> atom; // usado se op == P_ATOM
    std::unique_ptr<ModalFormula> left;
    std::unique_ptr<ModalFormula> right;

    // Construtores auxiliares
    static ModalFormula patom(const CPLFormula& phi);
    static ModalFormula unary(ModalConnective op, ModalFormula operand);
    static ModalFormula binary(ModalConnective op, ModalFormula lhs, ModalFormula rhs);

    void collectPropVars(std::unordered_map<std::string, int>& propVarToId) const;
    std::string toString() const;
    std::unique_ptr<ModalFormula> clone() const;
};

#endif // FORMULA_HPP


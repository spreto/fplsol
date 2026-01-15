#include "formula.h"

// ------------------------
// CPLFormula Implementação
// ------------------------

CPLFormula::CPLFormula() = default;

CPLFormula CPLFormula::variable(const std::string& name) {
    CPLFormula f;
    f.op = CPLConnective::VAR;
    f.var = name;
    return f;
}

CPLFormula CPLFormula::unary(CPLConnective op, CPLFormula operand) {
    CPLFormula f;
    f.op = op;
    f.left = std::make_unique<CPLFormula>(std::move(operand));
    return f;
}

CPLFormula CPLFormula::binary(CPLConnective op, CPLFormula lhs, CPLFormula rhs) {
    CPLFormula f;
    f.op = op;
    f.left = std::make_unique<CPLFormula>(std::move(lhs));
    f.right = std::make_unique<CPLFormula>(std::move(rhs));
    return f;
}

void CPLFormula::collectPropVars(std::unordered_map<std::string, int>& propVarToId) const {
    using C = CPLConnective;

    if (op == C::VAR) {
        if (!propVarToId.count(var)) {
            int id = static_cast<int>(propVarToId.size());
            propVarToId[var] = id;
        }
    } else if (op == C::NOT) {
        if (left) left->collectPropVars(propVarToId);  // NOT armazena em left
    } else {
        if (left) left->collectPropVars(propVarToId);
        if (right) right->collectPropVars(propVarToId);
    }
}

std::string CPLFormula::toString() const {
    if (cached) return cachedString;

    using C = CPLConnective;
    switch (op) {
        case C::VAR:
            cachedString = var;
            break;
        case C::NOT:
            cachedString = "¬(" + left->toString() + ")";
            break;
        case C::AND:
            cachedString = "(" + left->toString() + " ∧ " + right->toString() + ")";
            break;
        case C::OR:
            cachedString = "(" + left->toString() + " ∨ " + right->toString() + ")";
            break;
        case C::IMPLIES:
            cachedString = "(" + left->toString() + " → " + right->toString() + ")";
            break;
        case C::IFF:
            cachedString = "(" + left->toString() + " ↔ " + right->toString() + ")";
            break;
    }

    cached = true;
    return cachedString;
}

std::unique_ptr<CPLFormula> CPLFormula::clone() const {
    auto copy = std::make_unique<CPLFormula>();
    copy->op = this->op;
    copy->var = this->var;
    if (left)  copy->left  = left->clone();
    if (right) copy->right = right->clone();
    return copy;
}

// ---------------------------
// ModalFormula Implementação
// ---------------------------

ModalFormula ModalFormula::patom(const CPLFormula& phi) {
    ModalFormula f;
    f.op = ModalConnective::P_ATOM;
    f.atom = phi.clone();
    return f;
}

ModalFormula ModalFormula::unary(ModalConnective op, ModalFormula operand) {
    ModalFormula f;
    f.op = op;
    f.left = std::make_unique<ModalFormula>(std::move(operand));
    return f;
}

ModalFormula ModalFormula::binary(ModalConnective op, ModalFormula lhs, ModalFormula rhs) {
    ModalFormula f;
    f.op = op;
    f.left = std::make_unique<ModalFormula>(std::move(lhs));
    f.right = std::make_unique<ModalFormula>(std::move(rhs));
    return f;
}

void ModalFormula::collectPropVars(std::unordered_map<std::string, int>& propVarToId) const {
    using M = ModalConnective;

    if (op == M::P_ATOM) {
        if (atom) atom->collectPropVars(propVarToId);
    } else if (op == M::NOT) {
        if (left) left->collectPropVars(propVarToId);  // NOT armazena em left
    } else {
        if (left) left->collectPropVars(propVarToId);
        if (right) right->collectPropVars(propVarToId);
    }
}

std::string ModalFormula::toString() const {
    if (cached) return cachedString;

    using M = ModalConnective;
    switch (op) {
        case M::P_ATOM:
            cachedString = "P(" + atom->toString() + ")";
            break;
        case M::NOT:
            cachedString = "¬(" + left->toString() + ")";
            break;
        case M::AND:
            cachedString = "(" + left->toString() + " ∧ " + right->toString() + ")";
            break;
        case M::OR:
            cachedString = "(" + left->toString() + " ∨ " + right->toString() + ")";
            break;
        case M::IMPLIES:
            cachedString = "(" + left->toString() + " → " + right->toString() + ")";
            break;
        case M::IFF:
            cachedString = "(" + left->toString() + " ↔ " + right->toString() + ")";
            break;
        case M::OPLUS:
            cachedString = "(" + left->toString() + " ⊕ " + right->toString() + ")";
            break;
        case M::ODOT:
            cachedString = "(" + left->toString() + " ⊙ " + right->toString() + ")";
            break;
    }

    cached = true;
    return cachedString;
}

std::unique_ptr<ModalFormula> ModalFormula::clone() const {
    auto copy = std::make_unique<ModalFormula>();
    copy->op = this->op;

    if (atom)
        copy->atom = atom->clone();

    if (left)
        copy->left = left->clone();

    if (right)
        copy->right = right->clone();

    return copy;
}


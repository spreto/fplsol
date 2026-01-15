#include "linear_program.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

using namespace soplex;

LinearProgram::LinearProgram() {
    solver = std::make_unique<SoPlex>();
    solver->setIntParam(SoPlex::OBJSENSE, SoPlex::OBJSENSE_MINIMIZE);
    solver->setIntParam(SoPlex::VERBOSITY, SoPlex::VERBOSITY_ERROR);
}

LinearProgram::LinearProgram(const LinearProgram& other)
    : solver(std::make_unique<SoPlex>(*other.solver)),
      varNames(other.varNames),
      nameToIndex(other.nameToIndex) {}

int LinearProgram::addVariable(const std::string& name, double lb, double ub) {
    DSVector colVec;
    LPCol col(0.0, colVec, ub, lb); // upper, lower
    solver->addColReal(col);
    int index = static_cast<int>(varNames.size());
    varNames.push_back(name);
    nameToIndex[name] = index;
    return index;
}

void LinearProgram::addConstraint(const std::vector<std::pair<int, double>>& terms,
                                  int sense, double rhs) {
    DSVector rowVec;
    for (const auto& [idx, coeff] : terms)
        rowVec.add(idx, coeff);

    Real lhs = -infinity;
    Real ub =  infinity;

    if (sense == LE)      ub = rhs;
    else if (sense == GE) lhs = rhs;
    else if (sense == EQ) lhs = ub = rhs;
    else throw std::invalid_argument("Constraint with invalid sense.");

    solver->addRowReal(LPRow(lhs, rowVec, ub));
}

void LinearProgram::setObjective(const std::vector<std::pair<int, double>>& terms,
                                 bool minimize) {
    int n = solver->numCols();
    DVector obj(n);
    obj.clear();

    for (const auto& [i, c] : terms)
        obj[i] = c;

    solver->changeObjReal(obj);
    solver->setIntParam(SoPlex::OBJSENSE,
                        minimize ? SoPlex::OBJSENSE_MINIMIZE : SoPlex::OBJSENSE_MAXIMIZE);
}

void LinearProgram::addCoefficientToRow(int row, int varIdx, double value) {
    soplex::DSVector rowVec;
    solver->getRowVectorReal(row, rowVec);

    rowVec.add(varIdx, value);

    double lhs = solver->lhsReal(row);
    double rhs = solver->rhsReal(row);

    solver->changeRowReal(row, soplex::LPRow(lhs, rowVec, rhs));
}

void LinearProgram::setObjectiveCoefficient(int varIdx, double coeff) {
    solver->changeObjReal(varIdx, coeff);
}

void LinearProgram::setMinimizationObjective() {
    solver->setIntParam(soplex::SoPlex::OBJSENSE, soplex::SoPlex::OBJSENSE_MINIMIZE);
}

bool LinearProgram::solve() {
    // solver->writeFileReal("modelo.lp");
    auto status = solver->solve();


    // Pega os valores ótimos das variáveis
    std::vector<double> sol(solver->numCols());
    solver->getPrimalReal(sol.data(), solver->numCols());

    /*
    // Debug: imprime na tela
    std::cout << "=== Primal solution ===" << std::endl;
    for (int i = 0; i < solver->numCols(); i++) {
        std::cout << varNames[i] << " = " << sol[i] << std::endl;
    }

    std::cout << "Status do solver: ";
    switch (status) {
        case SPxSolver::OPTIMAL:     std::cout << "Ótimo encontrado.\n"; break;
        case SPxSolver::INFEASIBLE:  std::cout << "Problema inviável.\n"; break;
        case SPxSolver::UNBOUNDED:   std::cout << "Problema ilimitado.\n"; break;
        case SPxSolver::ABORT_TIME:  std::cout << "Aborto por tempo.\n"; break;
        case SPxSolver::ABORT_ITER:  std::cout << "Aborto por iterações.\n"; break;
        case SPxSolver::ABORT_VALUE: std::cout << "Aborto por instabilidade numérica.\n"; break;
        default:                     std::cout << "Outro (status = " << status << ").\n"; break;
    }
    */

    return status == SPxSolver::OPTIMAL;
}

double LinearProgram::getObjectiveValue() const {
    return solver->objValueReal();
}

double LinearProgram::getVariableValue(int index) const {
    DVector primal(solver->numCols());
    if (!solver->getPrimalReal(primal.get_ptr(), primal.dim()))
        throw std::runtime_error("Failed to obtain primal solution.");
    return primal[index];
}

int LinearProgram::getVarIndex(const std::string& name) const {
    auto it = nameToIndex.find(name);
    if (it == nameToIndex.end())
        throw std::runtime_error("Variable not found: " + name);
    return it->second;
}

std::set<std::string> LinearProgram::getBinaryVariableNames() const {
    std::set<std::string> binaries;
    for (const auto& [name, _] : nameToIndex)
        if (name.rfind("b(", 0) == 0)
            binaries.insert(name);
    return binaries;
}

std::vector<double> LinearProgram::getDuals() const {
    int m = solver->numRows();
    soplex::DVector duals(m);
    if (!solver->getDualReal(duals.get_ptr(), duals.dim()))
        throw std::runtime_error("Failed to obtain simplex (dual) multipliers.");

    std::vector<double> result(m);
    for (int i = 0; i < m; ++i)
        result[i] = duals[i];
    return result;
}

std::unique_ptr<LinearProgram> LinearProgram::clone() const {
    return std::make_unique<LinearProgram>(*this);
}

void LinearProgram::print(std::ostream& os) const {
    int n = solver->numCols();
    int m = solver->numRows();

    os << "=== Variáveis ===\n";
    for (int i = 0; i < n; ++i) {
        double lb = solver->lowerReal(i);
        double ub = solver->upperReal(i);
        os << "  [" << std::setw(2) << i << "] " << varNames[i]
           << " ∈ [" << lb << ", " << ub << "]\n";
    }

//    os << "\n=== Restrições ===\n";
    os << "\n=== Constraints ===\n";
    for (int r = 0; r < m; ++r) {
        DSVector rowVec;
        solver->getRowVectorReal(r, rowVec);
        os << "  [row " << r << "] ";

        bool first = true;
        for (int i = 0; i < rowVec.size(); ++i) {
            if (!first) os << " + ";
            os << rowVec.value(i) << "*" << varNames[rowVec.index(i)];
            first = false;
        }

        double lhs = solver->lhsReal(r);
        double rhs = solver->rhsReal(r);

        if (std::abs(lhs - rhs) < 1e-8) os << " == " << rhs;
        else if (lhs > -infinity)      os << " >= " << lhs;
        else if (rhs < infinity)       os << " <= " << rhs;
        else                           os << " (invalid constraint)";
        os << "\n";
    }

    os << "\n=== Total: " << n << " variables, " << m << " constraints ===\n";
}


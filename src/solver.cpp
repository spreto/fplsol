#include "solver.h"
#include "formula.h"
#include "linear_program.h"
#include <iostream>
#include <stdexcept>
#include <set>
#include <unordered_map>
#include <string>
#include <sstream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <optional>

using namespace std;

// ----------- Construtor -----------

FPSolver::FPSolver(vector<ModalFormula> formulas_, string inputFilename_, bool verbose_) :
    formulas(move(formulas_)),
    inputFilename(inputFilename_),
    verbose(verbose_)
{
    // cout << "Iniciando solver FP(Ł)..." << endl;
    cout << "Initiating FP(Ł) solver..." << endl;

    preprocess();
}

// ----------- Configura o PB solver -----------

void FPSolver::setPBOptions(bool usePBFlag, const string& pbSolverPath, const string& pbArguments) {
    usePB = usePBFlag;
    pbSolver = pbSolverPath;
    pbArg = pbArguments;

    string command = "which " + pbSolverPath + " > /dev/null";

    if (std::system(command.c_str()) != 0) {
        cout << "[Fusca] Column generation via exhaustive search." << endl;
        usePB = false;
    }
}

// ----------- Função auxiliar: coleta subfórmulas atômicas Pφ -----------

static void collectPatoms(const ModalFormula& f, vector<unique_ptr<CPLFormula>>& out) {
    using M = ModalConnective;
    if (f.op == M::P_ATOM && f.atom)
        out.push_back(f.atom->clone());
    else {
        if (f.left) collectPatoms(*f.left, out);
        if (f.right) collectPatoms(*f.right, out);
    }
}

// ----------- Pré-processamento -----------

void FPSolver::preprocess() {
    for (const auto& f : formulas)
        f.collectPropVars(propVarToId);

    varList.resize(propVarToId.size());
    for (const auto& [v, id] : propVarToId)
        varList[id] = v;

    // cout << "Detectadas " << propVarToId.size() << " variáveis proposicionais." << endl;
}

// ----------- Solver principal -----------

bool FPSolver::solve() {
    auto rootLP = make_unique<LinearProgram>();
    unordered_map<string, int> xVars, bVars;
    vector<unique_ptr<CPLFormula>> psiList;
    vector<int> probConstraintRows;

    for (const auto& f : formulas)
        encodeModalFormula(f, *rootLP, xVars, bVars);

    for (const auto& f : formulas) {
        string id = f.toString();
        if (!xVars.count(id))
            throw runtime_error("FP(Ł) formula not translated: " + id);
        rootLP->addConstraint({{xVars[id], 1.0}}, LinearProgram::EQ, 1.0);

    }

    for (const auto& f : formulas)
        collectPatoms(f, psiList);

    for (const auto& psiPtr : psiList) {
        ModalFormula patom = ModalFormula::patom(*psiPtr);
        string id = patom.toString();

        if (!xVars.count(id))
            throw runtime_error("Variable x(" + id + ") not found.");

        int xIdx = xVars.at(id);
        // Só o termo -x_{Pψᵢ}
        rootLP->addConstraint({{xIdx, -1.0}}, LinearProgram::EQ, 0.0);
        probConstraintRows.push_back(rootLP->numRows() - 1);
    }

    rootLP->addConstraint({}, LinearProgram::EQ, 1.0);
    int sumProbRow = rootLP->numRows() - 1;

    int numInitialRows = rootLP->numRows();
    vector<int> iVarIndices;

    for (int row = 0; row < numInitialRows; ++row) {
        string name = "i(" + to_string(row) + ")";
        int iIdx = rootLP->addVariable(name, 0.0, soplex::infinity);
        iVarIndices.push_back(iIdx);

        // Essa variável só entra com 1.0 na linha 'row'
        rootLP->addCoefficientToRow(row, iIdx, 1.0);
        // Também entra com 1.0 na função objetivo
        rootLP->setObjectiveCoefficient(iIdx, 1.0);
    }

    rootLP->setMinimizationObjective();

    auto inputRootLP = rootLP->clone();
    if (!isFeasible(psiList, propVarToId, move(inputRootLP), xVars, probConstraintRows, sumProbRow)) {
        if (verbose) cout << endl;
        cout << "UNSAT (infeasible relaxed problem)" << endl;
        return false;
    }

    set<string> binaries = rootLP->getBinaryVariableNames();
    vector<Branch> branches;
    branches.push_back(Branch{move(rootLP), {}});

    while (!branches.empty() && !binaries.empty()) {
        string b;
        b = *binaries.begin();
        binaries.erase(b);

        vector<Branch> next;

        for (auto& br : branches) {
            for (int val : {0, 1}) {
                auto newLP = br.lp->clone();
                newLP->addConstraint({{newLP->getVarIndex(b), 1.0}}, LinearProgram::EQ, val);
                auto inputNewLP = newLP->clone();
                if (isFeasible(psiList, propVarToId, move(inputNewLP), xVars, probConstraintRows, sumProbRow)) {
                    auto fixed = br.fixedBinaries;
                    fixed[b] = val;
                    next.push_back(Branch{move(newLP), fixed});
                }
            }
        }

        branches = move(next);
    }

    if (branches.empty()) {
        if (verbose) cout << endl;
        cout << "UNSAT (all branches closed)" << endl;
        return false;
    }

    if (verbose) cout << endl;
    cout << "SAT (open branch found)" << endl;

    cout << "\n==== MODAL ATOMS VALUATION ====\n";
    for (const auto& [id, val] : lastModalValues)
        cout << id << " = " << val << "\n";

    cout << "\n==== PROBABILITY DISTRIBUTION ====\n";
    for (size_t i = 0; i < lastProbDistribution.size(); ++i) {
        size_t w = 0;
        if (!usePB) {
            for (size_t j = 0; j < varList.size(); ++j)
                if (lastValuations.at(i).at(j))
                    w |= (1 << j);
        }
        cout << "p(" << (usePB ? i : w) << ") = " << lastProbDistribution[i] << "   (";
        for (size_t j = 0; j < varList.size(); ++j)
            cout << varList[j] << "=" << lastValuations.at(i).at(j) << (j + 1 < varList.size() ? ", " : "");
        cout << ")\n";
    }

    saveOutputToFile();  // escreve no arquivo .out

    return true;
}

// ----------- Tradução de fórmulas modais -----------

void FPSolver::encodeModalFormula(const ModalFormula& f,
                                  LinearProgram& lp,
                                  unordered_map<string, int>& xVars,
                                  unordered_map<string, int>& bVars) {
    using M = ModalConnective;
    string id = f.toString();
    if (xVars.count(id)) return;

    if (f.op == M::P_ATOM) {
        xVars[id] = lp.addVariable("x(" + id + ")", 0, 1);
        return;
    }

    encodeModalFormula(*f.left, lp, xVars, bVars);
    int leftX = xVars[f.left->toString()];

    int rightX = -1;
    if (f.right) {
        encodeModalFormula(*f.right, lp, xVars, bVars);
        rightX = xVars[f.right->toString()];
    }

    int xIdx = lp.addVariable("x(" + id + ")", 0, 1);
    xVars[id] = xIdx;

    int bIdx = -1;
    if (f.op != M::NOT) bVars[id] = bIdx = lp.addVariable("b(" + id + ")", 0, 1);

    switch (f.op) {
        case M::NOT:
            lp.addConstraint({{leftX, -1}, {xIdx, 1}}, LinearProgram::EQ, 1);
            break;
        case M::OPLUS:
            lp.addConstraint({{bIdx, 1}, {xIdx, -1}}, LinearProgram::LE, 0);
            lp.addConstraint({{xIdx, 1}}, LinearProgram::LE, 1);
            lp.addConstraint({{leftX, 1}, {rightX, 1}, {bIdx, -1}, {xIdx, -1}}, LinearProgram::LE, 0);
            lp.addConstraint({{leftX, -1}, {rightX, -1}, {xIdx, 1}}, LinearProgram::LE, 0);
            break;
        case M::ODOT:
            lp.addConstraint({{xIdx, 1}}, LinearProgram::GE, 0);
            lp.addConstraint({{xIdx, 1}, {bIdx, -1}}, LinearProgram::LE, 0);
            lp.addConstraint({{leftX, 1}, {rightX, 1}, {xIdx, -1}}, LinearProgram::GE, 1);
            lp.addConstraint({{leftX, 1}, {rightX, 1}, {bIdx, -1}, {xIdx, -1}}, LinearProgram::LE, 0);
            break;
        case M::AND:
            lp.addConstraint({{leftX, 1}, {bIdx, -1}, {xIdx, -1}}, LinearProgram::LE, 0);
            lp.addConstraint({{leftX, -1}, {xIdx, 1}}, LinearProgram::LE, 0);
            lp.addConstraint({{rightX, 1}, {bIdx, 1}, {xIdx, -1}}, LinearProgram::LE, 1);
            lp.addConstraint({{rightX, -1}, {xIdx, 1}}, LinearProgram::LE, 0);
            break;
        case M::OR:
            lp.addConstraint({{leftX, 1}, {xIdx, -1}}, LinearProgram::LE, 0);
            lp.addConstraint({{leftX, -1}, {xIdx, 1}, {bIdx, -1}}, LinearProgram::LE, 0);
            lp.addConstraint({{rightX, 1}, {xIdx, -1}}, LinearProgram::LE, 0);
            lp.addConstraint({{rightX, -1}, {xIdx, 1}, {bIdx, 1}}, LinearProgram::LE, 1);
            break;
        case M::IMPLIES:
            lp.addConstraint({{bIdx, 1}, {xIdx, -1}}, LinearProgram::LE, 0);
            lp.addConstraint({{xIdx, 1}}, LinearProgram::LE, 1);
            lp.addConstraint({{leftX, 1}, {rightX, -1}, {bIdx, 1}, {xIdx, 1}}, LinearProgram::GE, 1);
            lp.addConstraint({{leftX, 1}, {rightX, -1}, {xIdx, 1}}, LinearProgram::LE, 1);
            break;
        case M::IFF:
            lp.addConstraint({{leftX, 1}, {rightX, -1}, {bIdx, 2}, {xIdx, 1}}, LinearProgram::GE, 1);
            lp.addConstraint({{leftX, 1}, {rightX, -1}, {xIdx, 1}}, LinearProgram::LE, 1);
            lp.addConstraint({{leftX, 1}, {rightX, -1}, {bIdx, 2}, {xIdx, -1}}, LinearProgram::LE, 1);
            lp.addConstraint({{leftX, -1}, {rightX, 1}, {xIdx, 1}}, LinearProgram::LE, 1);
            break;
        default:
            // throw runtime_error("Operador modal desconhecido.");
            throw runtime_error("Unknown modal operator.");
    }
}

// ----------- Avaliação clássica de CPL -----------

double FPSolver::evaluateCPL(const CPLFormula& f, const unordered_map<string, bool>& val) {
    using C = CPLConnective;
    switch (f.op) {
        case C::VAR:     return val.at(f.var) ? 1.0 : 0.0;
        case C::NOT:     return 1.0 - evaluateCPL(*f.left, val);
        case C::AND:     return min(evaluateCPL(*f.left, val), evaluateCPL(*f.right, val));
        case C::OR:      return max(evaluateCPL(*f.left, val), evaluateCPL(*f.right, val));
        case C::IMPLIES: return max(1.0 - evaluateCPL(*f.left, val), evaluateCPL(*f.right, val));
        case C::IFF:
            return evaluateCPL(*f.left, val) == evaluateCPL(*f.right, val) ? 1.0 : 0.0;
    }
    // throw runtime_error("Operador CPL inválido.");
    throw runtime_error("Invalid CPL operator.");
}

double FPSolver::evaluateCPL(const CPLFormula& f,
                             const vector<string>& varList,
                             const vector<bool>& valuation) {
    unordered_map<string, bool> val;
    for (size_t i = 0; i < varList.size(); ++i)
        val[varList[i]] = valuation[i];
    return evaluateCPL(f, val);
}

// ----------- Viabilidade de restrições -----------

bool FPSolver::isFeasible(const vector<unique_ptr<CPLFormula>>& psiList,
                          const unordered_map<string, int>& propVarToId,
                          unique_ptr<LinearProgram> lp,
                          const unordered_map<string, int>& xVars,
                          const vector<int>& probConstraintRows,
                          int sumProbRow)
{
    vector<int> pVars;
    set<vector<bool>> usedValuations;
    vector<vector<bool>> lastValuationsLocal;
    int iter = 1;

    string name = "p(0)";
    int pIdx = lp->addVariable(name, 0.0, 1.0);

    // Avaliação da valoração 000...0 (tudo falso)
    vector<bool> zeroValuation(varList.size(), false);

    for (size_t i = 0; i < psiList.size(); ++i) {
        double val = evaluateCPL(*psiList[i], varList, zeroValuation);
        if (abs(val) > 1e-8)
            lp->addCoefficientToRow(probConstraintRows[i], pIdx, val);
    }

    // Linha da soma de probabilidades
    lp->addCoefficientToRow(sumProbRow, pIdx, 1.0);
    lastValuationsLocal.push_back(zeroValuation);

    // Marca valuation 000...0 como usada
    usedValuations.insert(zeroValuation);

    while (true) {
        if (verbose) {
            cout << "============== LINEAR PROGRAM ==============" << endl;
            lp->print(cout);
            cout << "============================================" << endl;
        }

        bool status = lp->solve();
        if (!status) {
            if (verbose)
                cout << "  [isFeasible] Infeasible LP in iteration " << iter << "\n";
            return false;
        }

        double obj = lp->getObjectiveValue();
            if (verbose)
                cout << "  [isFeasible] Iter " << iter << ", obj = " << obj << "\n";

        if (obj <= 0) {
            lastModalValues.clear();
            lastProbDistribution.clear();
            lastValuations = move(lastValuationsLocal);

            for (const auto& [id, idx] : xVars)
                lastModalValues[id] = lp->getVariableValue(idx);

            int idx, t = 0;
            for (int w = 0; w < lastValuations.size(); ++w) {
                while (true) {
                    try {
                        idx = lp->getVarIndex("p(" + to_string(t) + ")");
                        ++t;
                        break;
                    }
                    catch (...) {
                        ++t;
                    }
                }
                double pVal = lp->getVariableValue(idx);
                lastProbDistribution.push_back(pVal);
            }

            return true;  // Ótimo viável com custo 0
        }

        bool added = addNewProbabilisticCoherenceConstraint(
            psiList, propVarToId, *lp, xVars, probConstraintRows,
            sumProbRow, usedValuations, lastValuationsLocal
        );

        if (!added) {
            if (verbose)
                cout << "  [isFeasible] No valuations remaining.\n";
            return false;
        }

        ++iter;
    }
}

bool FPSolver::addNewProbabilisticCoherenceConstraint(
    const vector<unique_ptr<CPLFormula>>& psiList,
    const unordered_map<string, int>& propVarToId,
    LinearProgram& lp,
    const unordered_map<string, int>& xVars,
    const vector<int>& probConstraintRows,
    int sumProbRow,
    set<vector<bool>>& usedValuations,
    vector<vector<bool>>& lastValuationsLocal)
{
    int n = varList.size();
    int total = 1 << n;

    // Pega os multiplicadores simplex da solução corrente
    vector<double> duals = lp.getDuals();

    // Calcula os coeficientes da inequação de custo reduzido: ∑ dualᵢ·ψᵢ(w) + dual_soma
    vector<double> coeffs(n, 0.0);  // uma variável por posição booleana
    vector<double> weights;        // coeficientes dos ψᵢ

    for (size_t i = 0; i < psiList.size(); ++i)
        weights.push_back(duals[probConstraintRows[i]]);
    weights.push_back(duals[sumProbRow]); // soma total: +1 sempre

    if (usePB) {
        // Tenta usar PB-SAT
        optional<vector<bool>> valuationPB =
            findValuationPB(psiList, weights, usedValuations);

        if (valuationPB.has_value()) {
            const auto& valuation = *valuationPB;
            usedValuations.insert(valuation);
            lastValuationsLocal.push_back(valuation);

            // Gera coluna correspondente
            vector<pair<int, double>> column;

            for (size_t i = 0; i < psiList.size(); ++i) {
                double val = evaluateCPL(*psiList[i], varList, valuation);
                if (abs(val) > 1e-8)
                    column.emplace_back(probConstraintRows[i], val);
            }

            column.emplace_back(sumProbRow, 1.0);  // linha da soma

            string name = "p(" + to_string(usedValuations.size()-1) + ")";
            int pIdx = lp.addVariable(name, 0.0, 1.0);
            for (const auto& [rowIdx, coeff] : column)
                lp.addCoefficientToRow(rowIdx, pIdx, coeff);

            if (verbose)
//                cout << "  [PB-SAT] Coluna adicionada via PB solver.\n";
                cout << "  [PB-SAT] Column added via PB solver.\n";
            return true;
        }
        else {
            return false;
        }
    }

    // Fusca: busca completa por custo reduzido ≤ 0
    for (int w = 0; w < total; ++w) {
        vector<bool> valuation(n);
        for (int i = 0; i < n; ++i)
            valuation[i] = (w >> i) & 1;

        if (usedValuations.count(valuation))
            continue;

        usedValuations.insert(valuation);

        vector<pair<int, double>> column;

        for (size_t i = 0; i < psiList.size(); ++i) {
            double val = evaluateCPL(*psiList[i], varList, valuation);
            if (abs(val) > 1e-8)
                column.emplace_back(probConstraintRows[i], val);
        }

        column.emplace_back(sumProbRow, 1.0);  // soma

        double reducedCost = 0.0;
        for (const auto& [rowIdx, coeff] : column)
            reducedCost -= duals[rowIdx] * coeff;

        if (verbose) {
            cout << "Valuation: ";
            for (bool b : valuation) cout << b;
            cout << ", reduced cost: " << reducedCost << "\n";
        }

        if (reducedCost < 0) {
            lastValuationsLocal.push_back(valuation);
            string name = "p(" + to_string(w) + ")";
            int pIdx = lp.addVariable(name, 0.0, 1.0);
            for (const auto& [rowIdx, coeff] : column)
                lp.addCoefficientToRow(rowIdx, pIdx, coeff);
            if (verbose)
//                cout << "  [Fusca] Coluna adicionada com custo reduzido ≤ 0.\n";
                cout << "  [Fusca] Column added with reduced cost ≤ 0.\n";
            return true;
        }
    }

    return false;  // Nenhuma coluna encontrada */
}

// ----------- Geração de colunas via PB-SAT -----------

void FPSolver::writeOPBFile(const vector<unique_ptr<CPLFormula>>& psiList,
                            const vector<double>& duals,
                            const set<vector<bool>>& usedValuations,
                            const string& filename)
{
    ofstream out(filename);
    if (!out)
        throw runtime_error("Error opening writting file: " + filename);

    int auxVarCounter = varList.size();  // x0, x1, ..., xn-1 já existem
    vector<string> yVars;      // nomes das vars auxiliares para ψ_i

    auto newVar = [&]() -> string {
        return "x" + to_string(auxVarCounter++);
    };

    function<void(const CPLFormula&, string&, ostream&)> encodeFormula;

    encodeFormula = [&](const CPLFormula& f, string& outVar, ostream& os) {
        using C = CPLConnective;

        if (f.op == C::VAR) {
            outVar = "x" + to_string(
                distance(varList.begin(), find(varList.begin(), varList.end(), f.var))
            );
            return;
        }

        string l, r, y = newVar();

        if (f.left)
            encodeFormula(*f.left, l, os);
        if (f.right)
            encodeFormula(*f.right, r, os);

        switch (f.op) {
            case C::NOT:
                outVar = y;
                os << "+1*" << l << " +1*" << y << " >= 1;\n";
                os << "-1*" << l << " -1*" << y << " >= -1;\n";
                break;
            case C::AND:
                outVar = y;
                os << "+1*" << l << " -1*" << y << " >= 0;\n";
                os << "+1*" << r << " -1*" << y << " >= 0;\n";
                os << "-1*" << l << " -1*" << r << " +1*" << y << " >= -1;\n";
                break;
            case C::OR:
                outVar = y;
                os << "-1*" << l << " +1*" << y << " >= 0;\n";
                os << "-1*" << r << " +1*" << y << " >= 0;\n";
                os << "+1*" << l << " +1*" << r << "-1*" << y << " >= 0;\n";
                break;
            case C::IMPLIES:
                outVar = y;
                os << "-1*" << r << " +1*" << y << " >= 0;\n";
                os << "+1*" << l << " +1*" << y << " >= 1;\n";
                os << "-1*" << l << " +1*" << r << " -1*" << y << " >= -1;\n";
                break;
            case C::IFF:
                outVar = y;
                os << "-1*" << y << " -1*" << l << " +1*" << r << " >= -1\n";
                os << "-1*" << y << " +1*" << l << " -1*" << r << " >= -1\n";
                os << "-1*" << l << " -1*" << r << " +1*" << y << " >= -1\n";
                os << "+1*" << l << " +1*" << r << " +1*" << y << " >= 1\n";
                break;
            default:
//                throw runtime_error("Conectivo CPL não suportado.");
                throw runtime_error("Unsupported CPL operator.");
        }
    };

    // Codificação das fórmulas ψ_i
    for (size_t i = 0; i < psiList.size(); ++i) {
        string y;
        encodeFormula(*psiList[i], y, out);
        yVars.push_back(y);
    }

    // Não gerar valorações já usadas
    out << "* Used valuations\n";
    for (auto valuation : usedValuations) {
        int num = 0;
        int rhs = 1; // lado direito da desigualdade
        for (bool v : valuation) {
            if (v) {
                out << "-1*x" << num << " ";
                rhs -= 1;
            } else {
                out << "+1*x" << num << " ";
            }
            ++num;
        }
        out << ">= " << rhs << ";\n";
    }

    // Restrição de custo reduzido: sum d_i * y_i + d_sum >= 1
    out << "* Reduced-cost inequality\n";
    for (size_t i = 0; i < yVars.size(); ++i) {
        long coeff = static_cast<long>(round(duals[i] * 1e6));  // escala para inteiros
        if (coeff == 0) continue;
        out << (coeff > 0 ? "+" : "") << coeff << "*" << yVars[i] << " ";
    }

    long dSum = static_cast<long>(round(duals.back() * 1e6));
    out << ">= " << 1-dSum << ";\n";
}

optional<vector<bool>> FPSolver::findValuationPB(
    const vector<unique_ptr<CPLFormula>>& psiList,
    const vector<double>& coeffs,
    const set<vector<bool>>& usedValuations)
{
    filesystem::path inPath(inputFilename);
    filesystem::path parent = inPath.parent_path();
    string stem = inPath.stem().string();
    filesystem::path opbFilename = parent / ("pb_input_"  + stem + ".opb");
    filesystem::path tmpOutput  = parent / ("pb_output_" + stem + ".txt");

    writeOPBFile(psiList, coeffs, usedValuations, opbFilename);
    string command = pbSolver;
    if (pbArg != "")
        command += " " + pbArg;
    command += " " + opbFilename.string() + " > " + tmpOutput.string();
    system(command.c_str());

    ifstream in(tmpOutput);
    if (!in)
        throw runtime_error("Failed opening PB solver output file.");

    string line;
    bool found = false;
    vector<bool> valuation(varList.size(), false);

    while (getline(in, line)) {
        if (line.rfind("v ", 0) == 0) {  // linha começa com "v "
            istringstream iss(line.substr(2));  // ignora "v "
            string token;
            while (iss >> token) {
                if (token[0] == '-') continue;  // variável falsa
                if (token[0] == 'x') {
                    int idx = stoi(token.substr(1));
                    if (idx < static_cast<int>(varList.size()))
                        valuation[idx] = true;
                }
            }
            found = true;
        } else if (line == "s UNSATISFIABLE") {
            command = "rm " + opbFilename.string() + " " + tmpOutput.string();
            system(command.c_str());
            return nullopt;
        }
    }

    command = "rm " + opbFilename.string() + " " + tmpOutput.string();
    system(command.c_str());

    return found ? optional<vector<bool>>(valuation) : nullopt;
}

// ----------- Salva solução em arquivo -----------

void FPSolver::saveOutputToFile() {
    string outName = inputFilename.substr(0, inputFilename.find_last_of('.')) + ".out";
    ofstream out(outName);
    if (!out) {
        cerr << "Error opening output file: " << outName << "\n";
        return;
    }

    out << "======= MODAL ATOMS VALUATION ====\n";
    for (const auto& [id, val] : lastModalValues)
        out << id << " = " << val << "\n";

    out << "\n==== PROBABILITY DISTRIBUTION ====\n";
    for (size_t i = 0; i < lastProbDistribution.size(); ++i) {
        size_t w = 0;
        if (!usePB) {
            for (size_t j = 0; j < varList.size(); ++j)
                if (lastValuations.at(i).at(j))
                    w |= (1 << j);
        }
        out << "p(" << (usePB ? i : w) << ") = " << lastProbDistribution[i] << "   (";
        for (size_t j = 0; j < varList.size(); ++j)
            out << varList[j] << "=" << (lastValuations[i][j] ? "1" : "0") << (j + 1 < varList.size() ? ", " : "");
        out << ")\n";
    }

    out.close();
    cout << "\nResult saved in: " << outName << "\n";
}


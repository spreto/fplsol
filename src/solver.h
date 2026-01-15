#ifndef SOLVER_HPP
#define SOLVER_HPP

#include "formula.h"
#include "linear_program.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <map>
#include <optional>

class FPSolver {
public:
    // Construtor
    explicit FPSolver(std::vector<ModalFormula> formulas, std::string inputFilename, bool verbose);

    // Configura o PB solver
    void setPBOptions(bool usePB, const std::string& pbSolverPath, const std::string& pbArguments);

    // Executa o algoritmo de decisão
    bool solve();

private:
    // Nome do arquivo de entrada
    std::string inputFilename;

    // Configurações do PB solver
    bool usePB = true;
    std::string pbSolver = "minisat+";
    std::string pbArg = "";

    // Verborse mode
    bool verbose = false;

    // Fórmulas de entrada
    std::vector<ModalFormula> formulas;

    // Mapeamento de variáveis proposicionais para IDs numéricos
    std::unordered_map<std::string, int> propVarToId;

    // Número total de variáveis proposicionais
    int numVars;

    // Lista de variáveis
    std::vector<std::string> varList;

    // Estrutura auxiliar para ramificação
    struct Branch {
        std::unique_ptr<LinearProgram> lp;
        std::map<std::string, int> fixedBinaries;

        Branch(std::unique_ptr<LinearProgram> lp_, std::map<std::string, int> fixed)
            : lp(std::move(lp_)), fixedBinaries(std::move(fixed)) {}

        Branch(Branch&&) = default;
        Branch& operator=(Branch&&) = default;
        Branch(const Branch&) = delete;
        Branch& operator=(const Branch&) = delete;
    };

    // Informações da solução SAT
    std::unordered_map<std::string, double> lastModalValues;
    std::vector<double> lastProbDistribution;
    std::vector<std::vector<bool>> lastValuations;

    // Etapas principais
    void preprocess();

    void encodeModalFormula(const ModalFormula& formula,
                            LinearProgram& lp,
                            std::unordered_map<std::string, int>& xVars,
                            std::unordered_map<std::string, int>& bVars);

    double evaluateCPL(const CPLFormula& formula,
                       const std::unordered_map<std::string, bool>& valuation);
    double evaluateCPL(const CPLFormula& formula,
                       const std::vector<std::string>& varList,
                       const std::vector<bool>& valuation);

    bool isFeasible(const std::vector<std::unique_ptr<CPLFormula>>& psiList,
                    const std::unordered_map<std::string, int>& propVarToId,
                    std::unique_ptr<LinearProgram> lp,
                    const std::unordered_map<std::string, int>& xVars,
                    const std::vector<int>& probConstraintRows,
                    int sumProbRow);

    bool addNewProbabilisticCoherenceConstraint(const std::vector<std::unique_ptr<CPLFormula>>& psiList,
                                                const std::unordered_map<std::string, int>& propVarToId,
                                                LinearProgram& lp,
                                                const std::unordered_map<std::string, int>& xVars,
                                                const std::vector<int>& probConstraintRows,
                                                int sumProbRow,
                                                std::set<std::vector<bool>>& usedValuations,
                                                std::vector<std::vector<bool>>& lastValuationsLocal);

    // Geração de colunas via PB-SAT
    void writeOPBFile(const std::vector<std::unique_ptr<CPLFormula>>& psiList,
                      const std::vector<double>& duals,
                      const std::set<std::vector<bool>>& usedValuations,
                      const std::string& filename);

    std::optional<std::vector<bool>> findValuationPB(const std::vector<std::unique_ptr<CPLFormula>>& psiList,
                                                     const std::vector<double>& coeffs,
                                                     const std::set<std::vector<bool>>& usedValuations);

    // Salva solução em arquivo
    void saveOutputToFile();
};

#endif // SOLVER_HPP


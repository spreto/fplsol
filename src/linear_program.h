#ifndef LINEAR_PROGRAM_HPP
#define LINEAR_PROGRAM_HPP

#include <soplex.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

class LinearProgram {
public:
    LinearProgram();
    LinearProgram(const LinearProgram& other);

    // Sentidos das restrições
    static constexpr int LE = 0; // ≤
    static constexpr int GE = 1; // ≥
    static constexpr int EQ = 2; // =

    // Definição do PL
    int addVariable(const std::string& name, double lb = 0.0, double ub = 1.0);
    void addConstraint(const std::vector<std::pair<int, double>>& terms,
                       int sense, double rhs);
    void setObjective(const std::vector<std::pair<int, double>>& terms,
                      bool minimize = true);
    void addCoefficientToRow(int row, int varIdx, double value);
    void setObjectiveCoefficient(int varIdx, double coeff);
    void setMinimizationObjective();

    // Retorna o número de linhas no PL
    int numRows() const { return solver->numRows(); }

    // Resolve o PL
    bool solve();

    // Acesso à solução
    double getObjectiveValue() const;
    double getVariableValue(int index) const;
    int getVarIndex(const std::string& name) const;
    std::set<std::string> getBinaryVariableNames() const;
    std::vector<double> getDuals() const;

    // Impressão e cópia
    void print(std::ostream& os) const;
    std::unique_ptr<LinearProgram> clone() const;

private:
    std::unique_ptr<soplex::SoPlex> solver;
    std::vector<std::string> varNames;
    std::unordered_map<std::string, int> nameToIndex;
};

#endif // LINEAR_PROGRAM_HPP


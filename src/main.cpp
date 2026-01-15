#include "parser.h"
#include "solver.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

int main(int argc, char* argv[]) {
    std::string inputFile;
    std::string pbsolverPath = "minisat+";
    std::string pbArguments = "";
    bool usePB = true;
    bool verbose = false;

    // Parsing de argumentos simples
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if ((arg == "-i" || arg == "--input") && i + 1 < argc) {
            inputFile = argv[++i];
        } else if (arg == "--no-pb") {
            usePB = false;
        } else if (arg == "--pbsolver" && i + 1 < argc) {
            pbsolverPath = argv[++i];
        } else if (arg == "--pbarg" && i + 1 < argc) {
            pbArguments = argv[++i];
        } else if (arg == "--verbose") {
            verbose = true;
        } else if (arg == "--help") {
            std::cout << "Use: fplsol -i <input file>\n";
            std::cout << "  --no-pb           Disables use of PB-SAT (uses only exhaustive search)\n";
            std::cout << "  --pbsolver <path> Path to PB-SAT solver (e.g. minisat+)\n";
            std::cout << "  --pbarg <arg>     Arguments to PB-SAT solver (e.g. -formula=1)\n";
            std::cout << "  --verbose         Verbose mode\n";
            std::cout << "  --help            Display this help\n";
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "Error: Input file not provided. Use -i <file>\n";
        return 1;
    }

    try {
        std::vector<ModalFormula> formulas = loadModalFormulasFromFile(inputFile);

        FPSolver solver(std::move(formulas), inputFile, verbose);
        solver.setPBOptions(usePB, pbsolverPath, pbArguments);
        bool sat = solver.solve();
        return sat ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}


# 🧠 FPLSol — A Solver for the FP(Ł) Logic

FPLSol is a complete solver for the logic **FP(Ł)**, a probabilistic modal logic based on **Łukasiewicz logic**.
It supports modal formulas over classical propositional formulas, interpreted under probabilistic semantics.

This solver uses **column generation** with **Simplex-based LP solving (via SoPlex)** and optionally **pseudo-Boolean solving (via minisat+)** for efficient probabilistic coherence checking.

---

## 🔧 Requirements

- A C++17 compiler (e.g., `g++ >= 9`)
- The **SoPlex** library (compiled with shared object support)
- The **GMP**, **TBB** and **zlib** libraries (required by SoPlex)
- [minisat+](http://minisat.se/MiniSat+.html) installed and available in the system as `minisat+` (optional but recommended)

---

## 🛠️ Building

To build FPLSol:

```
git clone https://github.com/spreto/fplsol.git
cd fplsol
make
```

The compiled binary will be placed in `bin/fplsol`.

---

## 🚀 Usage example

`./bin/fplsol -i examples/example1.txt`

### Optional flags

- `--no-pb` — disables the use of minisat+ (uses internal enumeration only)
- `--pbsolver <path>` — custom path to the PB-SAT solver (default: `minisat+`)
- `--help` — prints available options

---

## 📜 FP(Ł) Language Syntax

An input file contains one **modal formula** per line.

Modal formulas use `P(φ)` to denote probabilistic atoms over classical formulas `φ`.
Formulas are parsed with full support for standard logical connectives.

### Classical connectives (inside `P(...)`)

| Symbol | Description       | Example        |
|--------|-------------------|----------------|
| `¬`    | Negation          | `¬X`           |
| `∧`    | Conjunction       | `X ∧ Y`        |
| `∨`    | Disjunction       | `X ∨ Y`        |
| `→`    | Implication       | `X → Y`        |
| `↔`    | Bi-implication    | `X ↔ Y`        |

### Łukasiewicz connectives (for modal formulas)

| Symbol | Description          | Example                     |
|--------|----------------------|-----------------------------|
| `¬`    | Negation             | `¬P(X ∧ Y)`                 |
| `∧`    | Minimum (weak and)   | `P(X) ∧ P(Y)`               |
| `∨`    | Maximum (weak or)    | `P(X) ∨ P(Y)`               |
| `⊙`    | Strong conjunction   | `P(X) ⊙ P(Y)`               |
| `⊕`    | Strong disjunction   | `P(X) ⊕ P(Y)`               |
| `→`    | Implication          | `P(X) → P(Y)`               |
| `↔`    | Bi-implication       | `P(X) ↔ P(Y)`               |

### Variable names:

- Classical propositional variables must start with an uppercase letter (`X`, `Y`, `Z`)
- Digits are allowed (e.g., `X1`, `X42`, `X999`)

---

## 📤 Output

After running, FPLSol displays:
- Whether the instance is **SAT** or **UNSAT**
- The valuation of all modal atoms (e.g., `P(ψ) = 1.0`)
- The probabilistic distribution over classical worlds (valuation + associated weight)

---

## 📄 License

MIT License — © 2025 Sandro Preto

---

## 🏛️ Funding

- This work was carried out at the **Center for Artificial Intelligence (C4AI-USP)**, with support by the **São Paulo Research Foundation (FAPESP)**, grant #2019/07665-4, and by the IBM Corporation.
- This study was financed, in part, by the **São Paulo Research Foundation (FAPESP)**, Brasil. Process Number #2024/19144-7.

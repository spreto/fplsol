#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import random, csv
from concurrent.futures import ProcessPoolExecutor, as_completed
import subprocess, time, resource, os, signal

# =======================
# CONFIGURAÇÃO DO EXPERIMENTO
# =======================
OUTPUT_DIR = "exp-np-classical-3-t2"
os.makedirs(OUTPUT_DIR, exist_ok=True)
MAX_PARALLEL = int( 0.75*os.cpu_count() )    # número máximo de processos simultâneos
TIMEOUT = 2*60*60                            # timeout em segundos

NUM_VARIABLES = 120    # n: número de variáveis clássicas
NUM_INSTANCES = 500    # instâncias por ponto da grade

# Experimento: "np-modal", "np-classical"
EXPERIMENT_TYPE = "np-classical"

# Grade de parâmetros: np-modal
MODAL_CLAUSES_LIST = [ k for k in range(1,20+1,1) ]    # k

# Grade de parâmetros: np-classical
CLASSICAL_CLAUSES_LIST = [ m for m in range(30,960+1,30) ] # m: número de cláusulas na CNF clássica
l1 = 3                                                     # l1: número de literais modais na cláusula modal
l2 = 3                                                     # l2: número de cláusulas na simple L-clausal form formula

# Caminho do solver
FPL_SOLVER_CMD = "/var/tmp/spreto/fplsol/bin/fplsol"
PB_SOLVER_CMD = "/var/tmp/spreto/fplsol/bin/minisat+"

# =======================
# FUNÇÕES AUXILIARES
# =======================

def generate_classical_poly(n_vars):
    """
    Gera uma fórmula clássica polinomial agregada em uma única string.
    Cada fórmula é uma implicação de átomos,
    admitindo-se que os átomos podem TOP ou BOT,
    com probabilidade uniforme dos átomos serem sorteados.
    Retorna a fórmula como uma string.
    """
    variables = [f"X{i+1}" for i in range(n_vars)]
    antecedente_choices = variables + ["TOP"]
    consequente_choices = variables + ["BOT"]

    while True:
        ant = random.choice(antecedente_choices)
        cons = random.choice(consequente_choices)
        if ant == "TOP" and cons != "BOT":
            return cons
        elif cons == "BOT" and ant != "TOP":
            return f"¬{ant}"
        elif ant != "TOP" and cons != "BOT":
            return f"({ant} → {cons})"

def generate_classical_cnf(n_vars, m_clauses):
    """
    Gera uma CNF clássica agregada em uma única string.
    Cada cláusula contém 3 variáveis aleatórias,
    com probabilidade de 50% de serem negadas.
    Retorna a CNF como uma lista de cláusulas (strings).
    """
    variables = [f"X{i+1}" for i in range(n_vars)]
    clauses = []
    for _ in range(m_clauses):
        vars_in_clause = random.choices(variables, k=3)
        clause = []
        for v in vars_in_clause:
            lit = v if random.random() < 0.5 else f"¬{v}"
            clause.append(lit)
        # Parentiza a cláusula como ((lit1 ∨ lit2) ∨ lit3)
        clause_expr = f"(({clause[0]} ∨ {clause[1]}) ∨ {clause[2]})"
        clauses.append(clause_expr)
    return clauses

def apply_prob_operator(input):
    """
    Aplica o operador P(...) sobre uma CNF dada por uma lista de cláusulas
    ou sobre uma fórmula.
    A CNF é formada com conjunção ∧ entre todas as cláusulas.
    Retorna uma string com um átomo modal P(...)
    """
    if isinstance(input, list):
        cnf = input[0]
        for c in input[1:]:
            cnf = f"({cnf} ∧ {c})"
        return f"P({cnf})"
    else:
        return f"P({input})"

# -----------------------
# Nível modal / Łukasiewicz
# -----------------------
L_CLAUSES = [
    "(({0} ⊕ {1}) ⊕ {2})",
    "((¬{0} ⊕ {1}) ⊕ {2})",
    "(({0} ⊕ ¬{1}) ⊕ {2})",
    "(({0} ⊕ {1}) ⊕ ¬{2})",
    "((¬{0} ⊕ ¬{1}) ⊕ {2})",
    "((¬{0} ⊕ {1}) ⊕ ¬{2})",
    "(({0} ⊕ ¬{1}) ⊕ ¬{2})",
    "((¬{0} ⊕ ¬{1}) ⊕ ¬{2})",
    "(¬({0} ⊕ {1}) ⊕ {2})",
    "(¬({0} ⊕ {2}) ⊕ {1})",
    "({0} ⊕ ¬({1} ⊕ {2}))"
]

def generate_l_clauses(k_clauses, n_vars, m_clauses):
    """
    Gera k cláusulas Łukasiewicz.
    Cada l-cláusula contém 3 átomos modais P(...), cada um sobre uma CNF própria.
    """
    clauses = []
    for _ in range(k_clauses):
        lits = [apply_prob_operator(generate_classical_cnf(n_vars, m_clauses)) for _ in range(3)]
        template = random.choice(L_CLAUSES)
        clause = template.format(*lits)
        clauses.append(clause)
    return clauses

def generate_l_clauses_np_modal(k_clauses, n_vars):
    """
    Gera k cláusulas Łukasiewicz.
    Cada l-cláusula contém três átomos modais P(...), cada um sobre uma fórmula clássical polinomial.
    """
    clauses = []
    for _ in range(k_clauses):
        mvars = [apply_prob_operator(generate_classical_poly(n_vars)) for _ in range(3)]
        template = random.choice(L_CLAUSES)
        clause = template.format(*mvars)
        clauses.append(clause)
    return clauses

def combine_modal_variables(mvars):
    """
    Combina variáveis modais P(...) em uma l-cláusula simples.
    Cada variável modal P(...) tem 50% de probabilidade de ser negada.
    """
    if not mvars:
        return ""
    expr = mvars[0] if random.random() < 0.5 else f"¬{mvars[0]}"
    for v in mvars[1:]:
        expr = f"({expr} ⊕ {v})" if random.random() < 0.5 else f"({expr} ⊕ ¬{v})"
    return expr

def generate_simple_l_clauses_np_classical(m_clauses, n_vars, l1, l2):
    """
    Gera l2 cláusulas Łukasiewicz simples.
    Cada l-cláusula simples contém l1 átomos modais P(...), cada um sobre uma CNF clássica com m_clauses cláusulas sobre n_vars variáveis clássicas.
    """
    clauses = []
    for _ in range(l2):
        mvars = [apply_prob_operator(generate_classical_cnf(n_vars, m_clauses)) for _ in range(l1)]
        clause = combine_modal_variables(mvars)
        clauses.append(clause)
    return clauses

def combine_l_clauses(clauses):
    """
    Combina l-cláusulas com ∧ (conjunção fraca).
    """
    if not clauses:
        return ""
    expr = clauses[0]
    for c in clauses[1:]:
        expr = f"({expr} ∧ {c})"
    return expr

def write_instance_file(formula, instance_id, output_dir):
    filename = os.path.join(output_dir, f"instance_{instance_id}.fpl")
    with open(filename, "w", encoding="utf-8") as f:
        f.write(formula + "\n")
    return filename

def run_solver(instance_file, instance_id):
    cmd = [FPL_SOLVER_CMD, "--pbsolver", PB_SOLVER_CMD, "-i", instance_file]

    # forçar experimento com único core
    core = (os.cpu_count() - MAX_PARALLEL) + (instance_id % MAX_PARALLEL)
    cmd = ["taskset", "-c", str(core)] + cmd

    # tempo de CPU gasto até agora pelos filhos
    usage_before = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime

    try:
        # Abrimos o processo em novo grupo, para matar tudo depois em caso de timeout
        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            preexec_fn=os.setsid  # Cria novo grupo de processos
        )

        try:
            stdout, stderr = proc.communicate(timeout=TIMEOUT)

        except subprocess.TimeoutExpired:
            # Mata todo o grupo de processos (solver + sat solver)
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            return ("TIMEOUT", TIMEOUT, "")

        # tempo de CPU gasto pelos filhos depois
        usage_after = resource.getrusage(resource.RUSAGE_CHILDREN).ru_utime

        elapsed_user = usage_after - usage_before

        # Status SAT/UNSAT/ERROR
        if proc.returncode == 0:
            sat_status = "SAT"
        elif proc.returncode == 2:
            sat_status = "UNSAT"
        else:
            sat_status = "ERROR"

        return sat_status, elapsed_user, stdout

    except Exception as e:
        return ("ERROR", 0.0, f"Exception: {e}")

def run_instance_np_modal(instance_id, k, n_vars, output_dir):
    """Função para execução UMA instância np-modal."""
    random.seed(int(time.time() * 1e6) ^ os.getpid() ^ instance_id)
    l_clauses = generate_l_clauses_np_modal(k, n_vars)
    formula = combine_l_clauses(l_clauses)
    instance_file = write_instance_file(formula, instance_id, output_dir)
    sat_status, elapsed, _ = run_solver(instance_file, instance_id)
    return {
        "n": n_vars,
        "k": k,
        "instance": instance_id,
        "time": elapsed,
        "SAT/UNSAT": sat_status
    }

def run_instance_np_classical(instance_id, m, n_vars, l1, l2, output_dir):
    """Função para execução UMA instância np-classical."""
    random.seed(int(time.time() * 1e6) ^ os.getpid() ^ instance_id)
    simp_l_clauses = generate_simple_l_clauses_np_classical(m, n_vars, l1, l2)
    formula = combine_l_clauses(simp_l_clauses)
    instance_file = write_instance_file(formula, instance_id, output_dir)
    sat_status, elapsed, _ = run_solver(instance_file, instance_id)
    return {
        "n": n_vars,
        "m": m,
        "l1": l1,
        "l2": l2,
        "instance": instance_id,
        "time": elapsed,
        "SAT/UNSAT": sat_status
    }

def save_results_csv(results, filename):
    keys = results[0].keys()
    with open(filename, "w", newline="", encoding="utf-8") as f:
        dict_writer = csv.DictWriter(f, fieldnames=keys)
        dict_writer.writeheader()
        dict_writer.writerows(results)

# =======================
# LOOP PRINCIPAL
# =======================

def run_experiment():
    results = []
    instance_counter = 0
    tasks = []

    if EXPERIMENT_TYPE == "np-modal":
        with ProcessPoolExecutor(max_workers=MAX_PARALLEL) as executor:
            for k in MODAL_CLAUSES_LIST:
                for _ in range(NUM_INSTANCES):
                    instance_counter += 1
                    future = executor.submit(run_instance_np_modal, instance_counter, k, NUM_VARIABLES, OUTPUT_DIR)
                    tasks.append(future)

            for future in as_completed(tasks):
                try:
                    result = future.result()
                except Exception as e:
                    print(f"Erro na instância: {e}")
                    continue
                results.append(result)
                print(f"Concluída instância {result['instance']} — tempo {result['time']:.2f}s")

    elif EXPERIMENT_TYPE == "np-classical":
        with ProcessPoolExecutor(max_workers=MAX_PARALLEL) as executor:
            for m in CLASSICAL_CLAUSES_LIST:
                for _ in range(NUM_INSTANCES):
                    instance_counter += 1
                    future = executor.submit(run_instance_np_classical, instance_counter, m, NUM_VARIABLES, l1, l2, OUTPUT_DIR)
                    tasks.append(future)

            for future in as_completed(tasks):
                try:
                    result = future.result()
                except Exception as e:
                    print(f"Erro na instância: {e}")
                    continue
                results.append(result)
                print(f"Concluída instância {result['instance']} — tempo {result['time']:.2f}s")

    else:
        print("Experimento inválido. Escolha 'np-modal' ou 'np-classical'.")
        return

    csv_file = os.path.join(OUTPUT_DIR, f"results_{EXPERIMENT_TYPE}.csv")
    save_results_csv(results, csv_file)
    print(f"Experimento concluído. Resultados salvos em {csv_file}")

# =======================
# EXECUÇÃO
# =======================
if __name__ == "__main__":
    run_experiment()


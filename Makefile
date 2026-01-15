# === Configurações ===
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -lsoplex -lgmp -lgmpxx -ltbb -lz
SRC_DIR = src
BIN_DIR = bin
EXE = $(BIN_DIR)/fplsol

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
HEADERS = $(wildcard $(SRC_DIR)/*.h)
OBJECTS = $(SOURCES:.cpp=.o)

# === Alvo principal ===
all: $(EXE)

$(EXE): $(SOURCES) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

# === Limpar ===
clean:
	rm -rf $(BIN_DIR) *.o *~ core

# === Ajuda ===
help:
	@echo "Available targets:"
	@echo "  make           Compile the project"
	@echo "  make clean     Delete binaries and tmp files"


CXX := g++
CXXFLAGS := -Wall -Wextra -Werror -std=c++17 -pedantic -O2 -g

SRC_DIR := src
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
OUT_DIR := $(BUILD_DIR)/output
TARGET := $(BUILD_DIR)/main

SRC := $(wildcard $(SRC_DIR)/*.cpp)
OBJ := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC))

.PHONY: all run clean dirs

all: $(TARGET)

dirs:
	mkdir -p $(OBJ_DIR) $(OUT_DIR)

$(TARGET): $(OBJ) | dirs
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	@if [ -z "$(CASE)" ]; then \
		echo "Usage: make run CASE=data/ibm01.hgr"; \
		exit 1; \
	fi
	./$(TARGET) "$(CASE)"

clean:
	rm -rf $(BUILD_DIR)

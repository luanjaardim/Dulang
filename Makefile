CCP=g++
CFLAGS=-g -std=c++20 -Wextra -Wall -Werror -pedantic
SRC_DIR=modules
SRC=$(wildcard $(SRC_DIR)/*.cpp)
OBJS_DIR=objs
OBJS=$(SRC:$(SRC_DIR)/%.cpp=$(OBJS_DIR)/%.o)
TARGET=dulang
PROJ_DIR=$(HOME)/.dir_dulang

all: $(OBJS_DIR) $(PROJ_DIR) $(OBJS) $(SRC_DIR)/modules.o $(TARGET)

$(OBJS_DIR):
	mkdir $(OBJS_DIR)

$(PROJ_DIR):
	pwd > $(PROJ_DIR)

$(OBJS_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CCP) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/modules.o: $(OBJS)
	ld -r $(OBJS) -o $(SRC_DIR)/modules.o

$(TARGET): $(OBJS) $(TARGET).cpp
	$(CCP) $(CFLAGS) $(SRC_DIR)/modules.o $@.cpp -o $@

clean:
	rm -r $(OBJS_DIR) $(TARGET) $(SRC_DIR)/modules.o

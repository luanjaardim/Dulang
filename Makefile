CC=gcc
CFLAGS=-g -std=c99 -Wextra -Wall -Werror -pedantic
SRC_DIR=modules
SRC=$(wildcard $(SRC_DIR)/*.c)
OBJS_DIR=objs
OBJS=$(SRC:$(SRC_DIR)/%.c=$(OBJS_DIR)/%.o)
TARGET=dulang

all: $(OBJS_DIR) $(OBJS) $(SRC_DIR)/modules.o $(TARGET)

$(OBJS_DIR):
	mkdir $(OBJS_DIR)

$(OBJS_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SRC_DIR)/modules.o: $(OBJS)
	ld -r $(OBJS) -o $(SRC_DIR)/modules.o

$(TARGET): $(OBJS) $(TARGET).c
	$(CC) $(CFLAGS) $(SRC_DIR)/modules.o $@.c -o $@

clean:
	rm -r $(OBJS_DIR) $(TARGET) $(SRC_DIR)/modules.o

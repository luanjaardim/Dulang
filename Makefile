CC=gcc
CFLAGS=-g -std=c99 -Wextra -Wall -Werror -pedantic
SRC=modules
OBJS_DIR=objs

all: $(OBJS_DIR) dulang

#create a directory to keep object files
$(OBJS_DIR):
	mkdir $@

#craete a .o file at OBJS_DIR directory for every .c file on SRC directory
$(OBJS_DIR)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c $^ -o $(OBJS_DIR)/$*.o
#mv *.o ./$(OBJS_DIR)/

#linking all .o files
$(SRC)/modules.o: $(OBJS_DIR)/*.o
	ld -r -o $@ $^

#cleaniiiiiiiiiiiinnnnnnnnnnnnnnnngggggggggggg
clean:
	rm -r $(OBJS_DIR)
	rm $(SRC)/modules.o
	rm dulang

#compiling the dulang.c file with the modules.o file
dulang: $(SRC)/modules.o dulang.c
	gcc $^ -o $@

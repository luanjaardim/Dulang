#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "lib.h"
#include "parser.h"

#define DULANG_EXT_SIZE 6
#define ASM_EXT_SIZE 6

#define CMD_ERROR 256


int main(int argc, char **argv) {

    if(argc < 2) {
        fprintf(stderr, "Error! You must pass the program to be compiled as an argument!\n");
        exit(1);
    }

    const char *file = argv[1];
    int len = lenStr(file);
    if(lenStr(file) < DULANG_EXT_SIZE || strcmp(file+(len-DULANG_EXT_SIZE), ".dulan")) {
        //file + (len-DULANG_EXT_SIZE) points to where the extension should initiate
        fprintf(stderr, "Error! Expected a Dulang file!\n");
        exit(1);
    }

    int fileNameLen = len-DULANG_EXT_SIZE;
    char fileName[fileNameLen];
    memcpy(fileName, file, fileNameLen); //copy the name of the file to parse without the .dulan
    fileName[fileNameLen] = '\0'; //null terminator to the end of the string

    FILE *f = fopen(file, "r");
    if(f == NULL) {
        fprintf(stderr, "Error! File does not exists\n");
        exit(1);
    }

    TokenizedFile tokFile = readToTokenizedFile(f);
    //Every word is turned into Tokens, with informations that helps on parsing
    printTokenizedFile(tokFile);
    Expression *expr = parseExprBlock(tokFile);
    printExprBlock(expr, 0);

    if(fclose(f)) {
        fprintf(stderr, "Error! Could not close the file\n");
        exit(1);
    }
    f = NULL;


    //adding .asm as the extension of the input file
    char fileToCreate[fileNameLen+ASM_EXT_SIZE];
    sprintf(fileToCreate, "%s%s", fileName, ".asm");

    f = fopen(fileToCreate, "w");
    fprintf(f, "segment .text\n");
    fprintf(f, "global _start\n");
    fprintf(f, "_start:\n");

    //
    // Generate Dulang code
    //

    fprintf(f, ";;--end of execution, return 0\n");
    fprintf(f, "    mov rdi, 0\n");
    fprintf(f, "    mov rax, 0x3c\n");
    fprintf(f, "    syscall\n");

    if(fclose(f)) {
        fprintf(stderr, "Error! Could not close the file\n");
        exit(1);
    }

   /*
    * Free mem
    */
    destroyTokenizdFile(tokFile);
    destroyExprBlock(expr);

    //Compiling the nasm file
    len = lenStr(fileToCreate)+20;
    char command[len];
    sprintf(command, "nasm -felf64 %s", fileToCreate);
    if(system(command) == CMD_ERROR) {
        fprintf(stderr, "Error! Could not compile the .asm file\n");
        exit(1);
    }

    //Linking .o file, to create the executable
    memset(command, 0, len);
    sprintf(command, "ld %s.o -o %s", fileName, fileName);
    if(system(command) == CMD_ERROR) {
        fprintf(stderr, "Error! Could not link the .o file\n");
        exit(1);
    }

    return 0;
}

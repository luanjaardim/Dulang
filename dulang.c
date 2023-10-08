#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#define DULANG_EXT_SIZE 6
#define ASM_EXT_SIZE 6

#define CMD_ERROR 256

#define QUIT(msg) fprintf(stderr, (msg)); exit(1);

int main(int argc, char **argv) {

    if(argc < 2) {
        QUIT("Error! You must pass the program to be compiled as an argument!");
    }

    const char *file = argv[1];
    int len = strlen(file);
    if(strlen(file) < DULANG_EXT_SIZE || strcmp(file+(len-DULANG_EXT_SIZE), ".dulan")) {
        QUIT("Error! Expected a Dulang file!\n"); //file + (len-DULANG_EXT_SIZE) points to where the extension should initiate
    }

    int fileNameLen = len-DULANG_EXT_SIZE;
    char fileName[fileNameLen];
    memcpy(fileName, file, fileNameLen); //copy the name of the file to parse without the .dulan
    fileName[fileNameLen] = '\0'; //null terminator to the end of the string

    FILE *f = fopen(file, "r");
    if(f == NULL) { QUIT("Error! File does not exists\n"); }

    //
    // Parse file
    //

    if(fclose(f)) { QUIT("Error! Could not close the file\n"); }
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

    if(fclose(f)) { QUIT("Error! Could not close the file\n"); }

    len = strlen(fileToCreate)+20;
    char command[len];
    sprintf(command, "nasm -felf64 %s", fileToCreate);
    if(system(command) == CMD_ERROR) {
        QUIT("Error! Could not compile the .asm file\n");
    }

    memset(command, 0, len);
    sprintf(command, "ld %s.o -o %s", fileName, fileName);
    if(system(command) == CMD_ERROR) {
        QUIT("Error! Could not link the .o file\n");
    }

    return 0;
}

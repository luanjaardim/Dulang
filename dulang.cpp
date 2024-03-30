#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>

#include "modules/tokenizer.h"

using namespace std;

#define DULANG_EXT_SIZE 6
#define ASM_EXT_SIZE 6

#define CMD_ERROR 256

int main(int argc, char **argv) {

    if(argc < 2) {
        fprintf(stderr, "Error! You must pass the program to be compiled as an argument!\n");
        exit(1);
    }

    string file = argv[1];
    size_t pos = file.find(".dulan");
    if(pos == string::npos) {
        fprintf(stderr, "Error! Expected a Dulang file!\n");
        exit(1);
    }

    string fileName = file.substr(0, pos);

    TokenizedFile *tokFile = readToTokenizedFile(file.c_str());
    //Every word is turned into Tokens, with informations that helps on parsing
    printf("Tokenized file:\n");
    printTokenizedFile(*tokFile);
    exit(1);
    // ParsedFile pf = createParsedFile(&tokFile);
    /* for(int i = 0; i < (int)pf.qtdBlocks; i++) */
    /*     printLinkExprs(pf.blocks[i].head, 0); */

    //adding .asm as the extension of the input file
    string fileToCreate = fileName + ".asm";

    //TODO: Implement the function that generates the .asm file with a ifstream

    //Generating the .asm file and compiling it
    // generateDulangFile(f, &pf);

   /*
    * Free mem
    */
    destroyTokenizdFile(tokFile);
    // destroyParsedFile(&pf);

    //Compiling the nasm file
    string command = "nasm -felf64 " + fileToCreate;
    if(system(command.c_str()) == CMD_ERROR) {
        fprintf(stderr, "Error! Could not compile the .asm file\n");
        exit(1);
    }

    //Linking .o file, to create the executable
    command.clear();
    command = "ld " + fileName + ".o -o " + fileName;
    if(system(command.c_str()) == CMD_ERROR) {
        fprintf(stderr, "Error! Could not link the .o file\n");
        exit(1);
    }

    return 0;
}

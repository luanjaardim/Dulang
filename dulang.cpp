#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>

#include "modules/generator.h"

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

    //Every word is turned into Tokens, with informations that helps on parsing
    TokenizedFile *tokFile = readToTokenizedFile(file.c_str());
    // printTokenizedFile(*tokFile);

    //Initializing the grammar
    Grammar *gm = new Grammar();
    //Parsing the tokenized file
    ParsedFile *node = gm->parseTokenizedFile(tokFile);
    //Printing the AST
    // printAST(node, "");


    //Translating the AST to a .c file
    string fileToCreate = fileName + ".c";
    Generator *g = new Generator();
    g->translateFile(node, fileToCreate);

    delete g;
    delete gm;
    destroyTokenizdFile(tokFile);

    //Compiling the c file
    string command = "gcc " + fileToCreate + " -o " + fileName;
    if(system(command.c_str()) == CMD_ERROR) {
        fprintf(stderr, "Error! Could not compile the .c file\n");
        exit(1);
    }

    return 0;
}

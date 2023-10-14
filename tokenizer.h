#ifndef TOK_H_
#define TOK_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#define NUM_BUILTIN_WORDS 22
#define LOW_PRECEDENCE 5

typedef enum {
    WORD_TK, //any name created by the user(that does not matches any of the builtin types)
    INT_TK,  //any number (not floating point)
    STR_TK,  //string (surrounded by `"`)
    BUILTIN_TK,   //builtin tokens (+, -, fn, int, =, or, and, ==, ...)
    COUNT_TYPES
} TokenType;

typedef struct {
    TokenType type;
    int precedence;
} TkTypeAndPrecedence ;

typedef struct Token {
    size_t id, qtdChars;
    char *text;
    int l, c; //line and column
    TkTypeAndPrecedence typeAndPrecedence;
} Token;

typedef struct TokenizedLine {
    size_t qtdElements, capElements;
    Token *tk;
} TokenizedLine;

typedef struct TokenizedFile {
    size_t qtdLines, capLines;
    size_t currLine, currElem; //used for navigation
    TokenizedLine *lines;
} TokenizedFile;

void printTokenizedFile(TokenizedFile p);
TokenizedFile readToTokenizedFile(FILE *fd);
void destroyTokenizdFile(TokenizedFile tp);
TokenizedFile cloneTokenizedFile(const TokenizedFile tf);
const Token *currTokenizedFile(TokenizedFile tf);
const Token *nextTokenizedFile(TokenizedFile *tf);
const Token *peekTokenizedFile(TokenizedFile tf);
const Token *returnTokenizedFile(TokenizedFile *tf);
const Token *peekBackTokenizedFile(TokenizedFile tf);
int advanceLineTokenizdFile(TokenizedFile *tf);
size_t endOfCurrBlock(TokenizedFile tf);

void maybeRealloc(void **pnt, int *const cap, int newSize, size_t elementSize);
size_t lenStr(const char *const str);
int cmpStr(const char *const str1, const char *const str2);

#endif // TOK_H_

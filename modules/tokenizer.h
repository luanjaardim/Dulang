#ifndef TOK_H_
#define TOK_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "utils.h"

typedef struct {
    TokenType type;
    Precedence precedence;
} TkInfo;

typedef struct Token {
    size_t id, qtdChars;
    char *text;
    int l, c; //line and column
    TkInfo info;
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
void destroyTokenizdFile(TokenizedFile *tp);
TokenizedFile cloneTokenizedFile(const TokenizedFile tf);
Token *currToken(TokenizedFile tf);
Token *nextToken(TokenizedFile *tf);
Token *peekToken(TokenizedFile tf);
Token *returnToken(TokenizedFile *tf);
Token *peekBackToken(TokenizedFile tf);
int advanceLineTokenizdFile(TokenizedFile *tf);

struct endOfBlock {
  size_t lastId, lastLine;
};
struct endOfBlock endOfCurrBlock(TokenizedFile tf);

#define min(a, b) (a) < (b) ? (a) : (b)

#endif // TOK_H_

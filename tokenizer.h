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

typedef enum {
    NAME_TK, //any name created by the user(that does not matches any of the builtin types)
    INT_TK,  //any number (not floating point)
    STR_TK,  //string (surrounded by `"`)
    NUM_DIV, //after this every identifier represents a builtin word
    NUM_MUL,
    NUM_MOD,
    LOG_NOT,
    CMP_EQ,
    CMP_DIF,
    NUM_ADD,
    NUM_SUB,
    LOG_OR,
    LOG_AND,
    CMP_GE, //greater or equal
    CMP_LE,
    CMP_GT, //greater than
    CMP_LT,
    BIT_AND,
    BIT_OR,
    BIT_NOT,
    ASSIGN,
    FUNC,
    IF,
    ELSE,
    WHILE,
    FOR,
    PAR_OPEN,
    PAR_CLOSE,
    COUNT_OF_TK_TYPES
} TokenType;

typedef enum {
  COMPTIME_KNOWN = -1,
  USER_DEFINITIONS,
  BUILTIN_LOW_PREC,
  BUILTIN_SINGLE_OPERAND,
  BUILTIN_MEDIUM_PREC,
  BUILTIN_HIGH_PREC,
} Precedence;

typedef struct {
    TokenType type;
    Precedence precedence;
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

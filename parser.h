#ifndef PARSER_H_
#define PARSER_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "utils.h"
#include "tokenizer.h"
#include "node.h"

typedef Node Expression;

typedef struct {
  Token *tk;
  unsigned char toParse; //variable that knows if some expression is already parsed
} TokenToParse;

typedef struct {
  Expression *head, *tail;
} ExprBlock;

//High level blocks are blocks that are not inside any other one, just inside of the main scope
typedef ExprBlock HighLevelBlock;

typedef struct {
  int8_t entryPoint; //index for main function
  size_t qtdBlocks, capBlocks;
  HighLevelBlock *blocks;
} ParsedFile;

ParsedFile createParsedFile(TokenizedFile *tf);
void destroyParsedFile(ParsedFile *pf);
ExprBlock createExprBlock(TokenizedFile *tf);
void destroyExprBlock(ExprBlock *block);
Expression *parseExprLink(Expression *expr);
void printLinkExprs(Expression *expr, int layer);

#endif // PARSER_H_

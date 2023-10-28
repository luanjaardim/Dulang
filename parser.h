#ifndef PARSER_H_
#define PARSER_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "tokenizer.h"
#include "node.h"

#define PARENT_LINK 0//convention, NULL if does not have
#define LEFT_LINK 1  //represents the expression at the right of some expression
#define RIGHT_LINK 2 //represents the expression at the left of some expression
#define CHILD(pos) RIGHT_LINK + pos
//any other number for links are it's childs

typedef Node Expression;

typedef struct {
  Token *tk;
  unsigned char toParse; //variable that knows if some expression is already parsed
} TokenToParse;

typedef struct {
  Expression *head, *tail;
} ExprBlock;

//High level blocks are blocks that are not inside any other one, just inside of the main scope
typedef struct {
  ExprBlock block;
} HighLevelBlock;

typedef struct {
  u_int8_t entryPoint; //index for main function
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

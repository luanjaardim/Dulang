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
  Expression *exprs;
} Functions;

typedef struct {
  size_t entryPoint; //index for main function
  Functions *funcs;
} ParsedFile;

Expression *parseExprBlock(TokenizedFile tf);
Expression *createExprBlock(TokenizedFile tf);
void printLinkExprs(Expression *expr, int layer);
void destroyExprBlock(Expression *expr);

#endif // PARSER_H_

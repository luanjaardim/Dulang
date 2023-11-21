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
#include "map.h"

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

//This is the element of the declaredFuncs map, it stores the number of arguments the function receives
//and the id of the function name
typedef struct {
  int qtdArgs, id;
} pairFunc;

typedef struct {
  int8_t entryPoint; //index for main function
  size_t qtdBlocks, capBlocks;
  HighLevelBlock *blocks;
  //Map of declared functions, contains the id of the function as key and the number of arguments it receives
  Map *declaredFuncs;
} ParsedFile;

ParsedFile createParsedFile(TokenizedFile *tf);
void destroyParsedFile(ParsedFile *pf);
ExprBlock createExprBlock(TokenizedFile *tf, Map *declaredFuncs);
void destroyExprBlock(ExprBlock *block);
void parseBlocks(TokenizedFile *tf, ParsedFile *pf);
Expression *parseExprLink(Expression *expr, Map *declaredFuncs);
void printLinkExprs(Expression *expr, int layer);
void deleteData(void *data); //deletes the data of a node

#endif // PARSER_H_

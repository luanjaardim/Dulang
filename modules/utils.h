#ifndef UTILS_H_
#define UTILS_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <limits>
#include <map>
#include <utility>
#include <algorithm>
#include <numeric>
#include <regex>

using namespace std;

#define PARENT_LINK 0//convention, NULL if does not have
#define LEFT_LINK 1  //represents the expression at the right of some expression
#define RIGHT_LINK 2 //represents the expression at the left of some expression
#define CHILD(pos) (RIGHT_LINK + pos)
//any other number for links are it's childs
#define SYSCALL_ARGS 7 // TODO: search how to get the return of a syscall

typedef enum {
  TK_NAME, //any name created by the user(that does not matches any of the builtin types)
  TK_INT,  //any number (not floating point)
  TK_STR,  //string (surrounded by `"`)
  TK_CHAR,
  TK_FLOAT,
  TK_INLINE_C, //inline c code (surrounded by "`")

  MARKER, //used only for divide the generic tokens(above) from the builtin words(below)

  //numeric operations
  TK_NUM_ADD,
  TK_NUM_SUB,
  TK_NUM_DIV,
  TK_NUM_MUL,
  TK_NUM_MOD,

  //logical operations
  TK_LOG_NOT,
  TK_LOG_OR,
  TK_LOG_AND,
  TK_LOG_EQ,
  TK_LOG_NE,
  TK_LOG_GE,
  TK_LOG_LE,
  TK_LOG_GT,
  TK_LOG_LT,

  //bitwise operations
  TK_BIT_NOT,
  TK_BIT_OR,
  TK_BIT_AND,
  TK_BIT_SHIFT_L,
  TK_BIT_SHIFT_R,
  TK_BIT_XOR,

  //types
  TK_TYPE_BYTE,
  TK_TYPE_UBYTE, 
  TK_TYPE_INT,
  TK_TYPE_UINT,
  TK_TYPE_FLOAT,
  TK_TYPE_NONE,
  TK_TYPE_REF,
  TK_TYPE_DEREF,
  TK_TYPE_FN_ARROW,
  TK_TYPE_TAG_UNION,
  TK_TYPE_COMPOUND,
  TK_TYPE_PARSE,

  //statements
  TK_BLOCK_FUNC,
  TK_BLOCK_TYPE,
  TK_BLOCK_IF,
  TK_BLOCK_ELSE,
  TK_BLOCK_WHILE,
  TK_BLOCK_LOOP,
  TK_BLOCK_FOR,
  TK_BLOCK_LOAD,
  TK_BLOCK_EMBED,
  TK_BLOCK_SKIP,
  TK_BLOCK_STOP,
  TK_BLOCK_BACK,
  TK_BLOCK_MATCH,

  //assignment keywords
  TK_ASSIGN,
  TK_VARIABLE,
  TK_CONSTANT,
  TK_SUM_ASSIGN,
  TK_SUB_ASSIGN,
  TK_MUL_ASSIGN,
  TK_DIV_ASSIGN,
  TK_MOD_ASSIGN,
  TK_INC_ASSIGN,
  TK_DEC_ASSIGN,

  //symbols
  TK_CUR_BRA_OPEN,
  TK_CUR_BRA_CLOSE,
  TK_SQR_BRA_OPEN,
  TK_SQR_BRA_CLOSE,
  TK_ROU_BRA_OPEN,
  TK_ROU_BRA_CLOSE,
  TK_END_BAR,
  TK_COLON,
  TK_COMMA,
  TK_DOT,
  TK_QUEST,
  TK_EXCLA,
  TK_SEMICOLON,
  TK_DOUB_SEMICOLON,
  TK_FN_RETURN,
  TK_NEW_LINE,
  TK_EOF,

  COUNT_OF_TK_TYPES
} TokenType;

struct Position {
  size_t l, e;
  bool found;
  Position(size_t l, size_t e) : l(l), e(e) { found = true; }
  Position() { found = false; }

  void goToPos(Position p) { l = p.l; e = p.e; }
  bool sameLine(Position p) { return l == p.l; }
  bool equals(Position p) { return l == p.l && e == p.e; }
  bool isBefore(Position p) { return l < p.l || (l == p.l && e < p.e); }
  bool isAfter(Position p) { return l > p.l || (l == p.l && e > p.e); }

  void print() { printf("Line: %lu, Element: %lu, Found: %s\n", l, e, found ? "True" : "False"); }
};

void maybeRealloc(void **pnt, int *const cap, int newSize, size_t elementSize);
size_t lenStr(const char *const str);
int cmpStr(const char *const str1, const char *const str2);
void swap(void *a, void *b, size_t size);
void insertIntToStr(FILE *f);

#endif // UTILS_H_

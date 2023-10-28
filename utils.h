#ifndef UTILS_H_
#define UTILS_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#define PARENT_LINK 0//convention, NULL if does not have
#define LEFT_LINK 1  //represents the expression at the right of some expression
#define RIGHT_LINK 2 //represents the expression at the left of some expression
#define CHILD(pos) RIGHT_LINK + pos
//any other number for links are it's childs

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

void maybeRealloc(void **pnt, int *const cap, int newSize, size_t elementSize);
size_t lenStr(const char *const str);
int cmpStr(const char *const str1, const char *const str2);
void swap(void *a, void *b, size_t size);

#endif // UTILS_H_

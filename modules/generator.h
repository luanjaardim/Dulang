#ifndef GENERATOR_H_
#define GENERATOR_H_

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "parser.h"
#include "map.h"
#include "utils.h"

typedef struct {
    Map *var_map;
    /* Map *func_map; */
    int currConditional;
    int currLoop;
} Generator;

void generateDulangFile(FILE *f, ParsedFile *pf);
void translateExpression(FILE *f, Expression *expr, Generator g);

#endif // GENERATOR_H_

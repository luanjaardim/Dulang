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
#include "bt_map.h"
#include "utils.h"

void generateDulangFile(FILE *f, ParsedFile *pf);
void translateExpression(FILE *f, Expression *expr);

#endif // GENERATOR_H_

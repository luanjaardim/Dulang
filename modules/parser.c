#include "parser.h"

INIT_NODE_TYPE(expr, TokenToParse)

Expression *createExpression(Token *tk) {
  //copying the token to a new one, to avoid problems with the tokenized file
  Token *tmpTk = malloc(sizeof(Token));
  memcpy(tmpTk, tk, sizeof(Token));
  tmpTk->text = malloc(tmpTk->qtdChars);
  memcpy(tmpTk->text, tk->text, tmpTk->qtdChars);

  TokenToParse tmpToken = {tmpTk, 1 };
  Expression *tmp = expr_node_create(tmpToken);
  node_set_link(tmp, NULL); //parent
  node_set_link(tmp, NULL); //left
  node_set_link(tmp, NULL); //right
  return tmp;
}

//This map will store the previously defined functions to take exactly the number of arguments
//it receives on parsing
Map declaredFuncs;

Expression *getRightAsChild(Expression *expr, int pos_child) {
  Expression *right = node_get_neighbour(expr, RIGHT_LINK);
  if(right) {
    node_swap_neighbours(expr, right, RIGHT_LINK, RIGHT_LINK);
    node_remove_link_at(right, RIGHT_LINK); node_remove_link_at(right, LEFT_LINK);
    node_set_double_link_at(expr, right, pos_child, PARENT_LINK);
  }
  else
    return NULL; //fail

  return right; //success
}

int isType(Token *tk) {
  switch(tk->info.type) {
    case TYPE_INT:
    case TYPE_STR:
      return 1; //true
    default:
      return 0; //false
  }
}

/*
 * Generates the expression AST of the expressions linked list
*/
Expression *parseExprLink(Expression *expr) {
  /* printLinkExprs(expr, 0); */
  /* printf("-------------------------------------------------------------\n"); */
  if(expr == NULL) return NULL;

  //starting from user definitios to find functions
  for(int i = USER_DEFINITIONS; i <= BUILTIN_HIGH_PREC && expr_node_get_value(expr).toParse; i++) {
    /* while(node_get_neighbour(expr, PARENT_LINK)) expr = node_get_neighbour(expr, PARENT_LINK); */

    Expression *tmpExpr = expr, *right = NULL, *left = NULL;

    TokenToParse leftTk, rightTk;
    Token *tmpToken, *leftToken, *rightToken;
    unsigned char leftIsParsed, rightIsParsed;
    do {
      //if fn expect a name, check if the name already exists, else add it to the map of functions
      //then expect ':' or '|', if ':' is found expect args types and names, if '|' there are no args
      //if the name is main it's the entry point
      //
      //if it's a name check if exists on map of functions and variables, expect for '='
      //if '=' is found, add the name to the map of variables
      //
      //the map of functions must have the name as the key and the label that will be generated as the value
      //the map of variables must have the name as the key and the offset from rbp to access it
      tmpToken = expr_node_get_value(tmpExpr).tk;
      /* printf("token: %s\n", tmpToken->text); */

      if(i == tmpToken->info.precedence && expr_node_get_value(tmpExpr).toParse) {
        /* printf("entrou: %s prec: %d\n", tmpToken->text, tmpToken->info.precedence); */
        left = node_get_neighbour(tmpExpr, LEFT_LINK);
        right = node_get_neighbour(tmpExpr, RIGHT_LINK);

        if(left) {
          leftTk = expr_node_get_value(left);
          leftToken = leftTk.tk; leftIsParsed = !leftTk.toParse;
        }
        else {
          leftToken = NULL;
          leftIsParsed = -1;
        }
        if(right) {
          rightTk = expr_node_get_value(right);
          rightToken = rightTk.tk; rightIsParsed = !rightTk.toParse;
        }
        else {
          rightToken = NULL;
          rightIsParsed = -1;
        }

        switch(tmpToken->info.precedence) {
          case USER_DEFINITIONS:
          {
            int tmp;
            //if it's a function name, we must change it's precedence
            if(map_get_value(&declaredFuncs, (void **)&tmpToken, &tmp)) {
              tmpToken->info.precedence = USER_FUNCTIONS;
            }
          }
            break;
          case BUILTIN_LOW_PREC:
            //case for '*' '/' '%'
            // it will only acept neighbours tokens that has the same or lower precedence, or if they
            // are already parsed
            if(leftToken && rightToken) {
              if((!leftIsParsed && leftToken->info.precedence > BUILTIN_LOW_PREC)
               ||(!rightIsParsed && rightToken->info.precedence > BUILTIN_LOW_PREC)) {
                  fprintf(stderr, "%s operation has invalid operands: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                  exit(1);
              }
            }
            else {
                fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
            }

            goto getLeftAndRightNeighbours;
            break;
          case BUILTIN_SINGLE_OPERAND:
            //unary operations, take just one arg after it
            switch(tmpToken->info.type) {
              //these two doesn't need any arg, we just need to set as parsed
              case SKIP_TK:
              case STOP_TK:
                expr_node_set_value(tmpExpr, (TokenToParse){expr_node_get_value(tmpExpr).tk, 0});
              break;
              default:
                getRightNeighbour:
                //get the first element after it as CHILD(1)
                if(right) {
                  node_swap_neighbours(tmpExpr, right, RIGHT_LINK, RIGHT_LINK);
                  node_remove_link_at(right, RIGHT_LINK); node_remove_link_at(right, LEFT_LINK);
                  node_set_double_link_at(tmpExpr, right, CHILD(1), PARENT_LINK);
                  //set as already parsed
                  expr_node_set_value(tmpExpr, (TokenToParse){expr_node_get_value(tmpExpr).tk, 0});
                }
                else {
                  fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                  exit(1);
                }
              break;
            }
            break;
          case BUILTIN_MEDIUM_PREC:
              if(leftToken && rightToken) {
                if((!leftIsParsed && leftToken->info.precedence > BUILTIN_MEDIUM_PREC)
                ||(!rightIsParsed && rightToken->info.precedence > BUILTIN_MEDIUM_PREC)) {
                    /* printLinkExprs(tmpExpr, 0); */
                  if(right) printf("right: %s\n", expr_node_get_value(right).tk->text);
                  if(left) printf("left: %s\n", expr_node_get_value(left).tk->text);
                    fprintf(stderr, "%s operation has invalid operands: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                    exit(1);
                }
              }
              else {
                  if(right) printf("right: %s\n", expr_node_get_value(right).tk->text);
                  if(left) printf("left: %s\n", expr_node_get_value(left).tk->text);
                  fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                  exit(1);
              }

              goto getLeftAndRightNeighbours;
          break;

          case USER_FUNCTIONS:
          {
            int tmp;
            if(map_get_value(&declaredFuncs, (void **) &tmpToken, &tmp) == 0) {
              fprintf(stderr, "Trying to use a undeclared function\n");
              exit(1);
            }
            /* printf("%s com %d argumentos\n", tmpToken->text, tmp); */
            int child = 0;
            while(tmp--) {
              if(node_get_neighbour(tmpExpr, RIGHT_LINK) == NULL) {
                fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }
              Expression *tmpRight = parseExprLink(node_get_neighbour(tmpExpr, RIGHT_LINK));
              node_swap_neighbours(tmpExpr, tmpRight, RIGHT_LINK, RIGHT_LINK);
              node_remove_link_at(tmpRight, LEFT_LINK); node_remove_link_at(tmpRight, RIGHT_LINK);
              node_set_double_link_at(tmpExpr, tmpRight, CHILD(++child), PARENT_LINK);
            }
            expr_node_set_value(tmpExpr, (TokenToParse){expr_node_get_value(tmpExpr).tk, 0});
          }
            break;
          case BUILTIN_HIGH_PREC:
            switch(tmpToken->info.type) {
              case ASSIGN:
              if(right && left)
                goto getLeftAndRightNeighbours;
              else {
                fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }
                break;
              case WHILE_TK:
              case IF_TK:
              {
                conditionAndBodyAsChilds:
                if(right == NULL) {
                  fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                  exit(1);
                }
                Expression *condition = right;
                while(right) {
                  if(expr_node_get_value(right).tk->info.type == END_BAR) {
                    //removing the END_BAR
                    node_remove_link_at(node_get_neighbour(right, LEFT_LINK), RIGHT_LINK);
                    right = node_remove_link_at(right, RIGHT_LINK);

                    if(!right) {
                      fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                      exit(1);
                    }
                    node_remove_link_at(right, LEFT_LINK);
                    break;
                  }
                  right = node_get_neighbour(right, RIGHT_LINK);
                }
                condition = parseExprLink(condition);
                right = parseExprLink(right);
                if(condition == NULL || right == NULL) {
                  fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                  exit(1);
                }
                node_remove_link_at(tmpExpr, RIGHT_LINK);
                node_set_link(tmpExpr, NULL); node_set_link(tmpExpr, NULL);
                node_set_double_link_at(tmpExpr, condition, CHILD(1), PARENT_LINK);
                node_set_double_link_at(tmpExpr, right, CHILD(2), PARENT_LINK);
              }
                break;
              case ELSE_TK:
                if(right) {
                  node_set_link(tmpExpr, NULL);
                  Token *tmpToken = expr_node_get_value(right).tk;
                  if(tmpToken->info.type == IF_TK && tmpToken->l == expr_node_get_value(tmpExpr).tk->l) {
                    node_swap_neighbours(tmpExpr, right, RIGHT_LINK, RIGHT_LINK);
                    /* printf("to delete: %s\n", expr_node_get_value(right).tk->text); */
                    node_delete(right, deleteData);
                    right = node_get_neighbour(tmpExpr, RIGHT_LINK);
                    goto conditionAndBodyAsChilds;
                  }
                  else {
                    node_change_neighbour_position(tmpExpr, RIGHT_LINK, CHILD(1));
                    right = parseExprLink(right);
                    node_change_neighbour_position(right, LEFT_LINK, PARENT_LINK);
                  }
                }
                else {
                  fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                  exit(1);
                }
                break;
              case SYSCALL_TK:
              {
                int paramCount = 0;
                while(1) {
                  if(paramCount > SYSCALL_ARGS) {
                    fprintf(stderr, "%s too many args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                    exit(1);
                  }
                  if(right == NULL) {
                    fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                    exit(1);
                  }
                  if(expr_node_get_value(right).tk->info.type == END_BAR) {
                    //removing the END_BAR
                    node_remove_link_at(node_get_neighbour(right, LEFT_LINK), RIGHT_LINK);
                    node_swap_neighbours(tmpExpr, right, RIGHT_LINK, RIGHT_LINK);
                    node_remove_link_at(right, RIGHT_LINK);
                    break;
                  }
                  node_remove_link_at(node_get_neighbour(right, LEFT_LINK), RIGHT_LINK);
                  node_remove_link_at(right, LEFT_LINK);
                  node_set_link(tmpExpr, right); //append to childs
                  right = node_get_neighbour(right, RIGHT_LINK);
                  paramCount++;
                }
                break;
              }
              case FUNC:

              if(right) {
                if(isType(expr_node_get_value(right).tk) == 0) {
                  fprintf(stderr, "%s operation has invalid operands: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                  exit(1);
                }
              } else{
                fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }

              //taking the return type and the function name
              int child = 1;
              getRightAsChild(tmpExpr, CHILD(child));;
              Expression *tmpRight = node_get_neighbour(tmpExpr, RIGHT_LINK);
              if(tmpRight) {
                Token *tmpRightToken = expr_node_get_value(tmpRight).tk;
                if(tmpRightToken->info.type == COLON) {
                  while(tmpRightToken->info.type != END_BAR) {
                    //discarting the COLON or the COMMA
                    node_swap_neighbours(tmpExpr, tmpRight, RIGHT_LINK, RIGHT_LINK);
                    node_delete(tmpRight, deleteData);

                    //the next parameter must be a function variable name
                    tmpRight = node_get_neighbour(tmpExpr, RIGHT_LINK);
                    if(tmpRight == NULL || isType(expr_node_get_value(tmpRight).tk) == 0) {
                      /* printf("sla: %s\n", expr_node_get_value(tmpRight).tk->text); */
                      fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                      exit(1);
                    }
                    getRightAsChild(tmpExpr, CHILD(++child));
                    tmpRight = node_get_neighbour(tmpExpr, RIGHT_LINK);
                    tmpRightToken = expr_node_get_value(tmpRight).tk;
                    if(tmpRight == NULL
                       || (tmpRightToken->info.type != COMMA
                       && tmpRightToken->info.type != END_BAR)) {
                      fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                      exit(1);
                    }
                  }
                }
                if(tmpRightToken->info.type == END_BAR) {
                  node_swap_neighbours(tmpExpr, tmpRight, RIGHT_LINK, RIGHT_LINK);
                  node_delete(tmpRight, deleteData);
                  tmpRight = node_get_neighbour(tmpExpr, RIGHT_LINK);
                  if(tmpRight) {
                    tmpRight = parseExprLink(tmpRight);
                    /* node_swap_neighbours(tmpExpr, tmpRight, RIGHT_LINK, RIGHT_LINK); */
                    node_remove_link_at(tmpRight, LEFT_LINK);
                    node_remove_link_at(tmpExpr, RIGHT_LINK);
                    node_set_link(tmpExpr, NULL);
                    node_set_double_link_at(tmpExpr, tmpRight, CHILD(++child), PARENT_LINK);
                  }
                  else {
                    fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                    exit(1);
                  }

                } else {
                  fprintf(stderr, "Not a valid function definition: %d, %d\n", tmpToken->l, tmpToken->c);
                  exit(1);
                }
              }
              else {
                fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }

              //here we should iterate tmpExpr till find the end of function definition, at |
              //storing informations about the type and number of params
              break;
              case BACK_TK:
              {
                if(right) {
                  Expression *tmpRight = parseExprLink(node_get_neighbour(tmpExpr, RIGHT_LINK));
                  node_remove_link_at(tmpExpr, RIGHT_LINK);
                  node_set_link(tmpExpr, NULL);
                  node_remove_link_at(tmpRight, LEFT_LINK); node_remove_link_at(tmpRight, RIGHT_LINK);
                  node_set_double_link_at(tmpExpr, tmpRight, CHILD(1), PARENT_LINK);
                } else {
                  fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                  exit(1);
                }
              }
              break;

                case PRINT_INT:
                  goto getRightNeighbour;
                  break;

              default:
                break;
            }

            break;

            //unreachble
            case COMPTIME_KNOWN:
            case SYMBOLS:
            case PRECEDENCE_COUNT:
              break;

            getLeftAndRightNeighbours:
              node_swap_neighbours(tmpExpr, left, LEFT_LINK, LEFT_LINK);
              node_swap_neighbours(tmpExpr, right, RIGHT_LINK, RIGHT_LINK);
              node_remove_link_at(left, RIGHT_LINK); node_remove_link_at(left, LEFT_LINK);
              node_remove_link_at(right, RIGHT_LINK); node_remove_link_at(right, LEFT_LINK);
              if(right) right = parseExprLink(right);
              if(left) left = parseExprLink(left);

              node_set_double_link_at(tmpExpr, left, CHILD(1), PARENT_LINK);
              node_set_double_link_at(tmpExpr, right, CHILD(2), PARENT_LINK);
              expr_node_set_value(tmpExpr, (TokenToParse){expr_node_get_value(tmpExpr).tk, 0});


        }
      }
      /* while(node_get_neighbour(tmpExpr, PARENT_LINK)) tmpExpr = node_get_neighbour(tmpExpr, PARENT_LINK); */

      tmpExpr = node_get_neighbour(tmpExpr, RIGHT_LINK);
    } while(tmpExpr);
  }
  //return the expression that is at the highest level
  while(node_get_neighbour(expr, PARENT_LINK)) expr = node_get_neighbour(expr, PARENT_LINK);
  expr_node_set_value(expr, (TokenToParse){expr_node_get_value(expr).tk, 0});
  /* printLinkExprs(expr, 0); */
  return expr;
}

/* ParsedFile parseTokenizedFile(TokenizedFile tf) { */

/*   return (ParsedFile){0}; */
/* } */

void printLinkExprs(Expression *expr, int layer) {
  if(!expr) return;

  const char *humanReadablePrec[PRECEDENCE_COUNT+1] = {"COMPTIME", "USER_DEF", "LOW_PREC", "BUILTIN_UNARY", "MEDIUM_PREC", "USER_FUNCTIONS", "HIGH_PREC", "SYMBOLS"};
  char space[layer+1];
  memset(space, ' ', layer);
  space[layer] = '\0';
  TokenToParse tp = expr_node_get_value(expr);
  printf("%s[text: %s, isParsed: %u, prec: %s]\n", space, tp.tk->text, !tp.toParse, humanReadablePrec[tp.tk->info.precedence + 1]);
  for(unsigned i = CHILD(1); i < node_get_num_neighbours(expr); i++) {
    /* printf("%s\n", expr_node_get_value(node_get_neighbour(expr, i))->text); */
    printLinkExprs(node_get_neighbour(expr, i), layer+1);
  }
  printLinkExprs(node_get_neighbour(expr, RIGHT_LINK), layer);
}

ExprBlock createExprBlockTill(TokenizedFile *tf, TokenType endType) {
  Expression *headExpr = createExpression(currToken(*tf));
  Expression *tailExpr = headExpr;
  Expression *tmp;
  nextToken(tf);
  while(currToken(*tf)->info.type != endType) {
    tmp = createExpression(currToken(*tf));
    node_set_double_link_at(tailExpr, tmp, RIGHT_LINK, LEFT_LINK);
    tailExpr = tmp;
    if(nextToken(tf) == NULL) break;
  }
  /* printLinkExprs(headExpr, 0); */
  Expression *parsedExpr = parseExprLink(headExpr);
  tailExpr = parsedExpr;
  while(node_get_neighbour(tailExpr, RIGHT_LINK)) tailExpr = node_get_neighbour(tailExpr, RIGHT_LINK);
  return (ExprBlock) {parsedExpr, tailExpr};
}

/*
 * Creates a linked list for all the tokens of the block
*/
ExprBlock createExprBlock(TokenizedFile *tf) {
  if(tf == NULL || currToken(*tf) == NULL) {
    fprintf(stderr, "Trying to create a expression block from a NULL tokenized file\n");
    exit(1);
  }
  Token *headToken = currToken(*tf);
  Expression *headExpr = createExpression(headToken);
  Expression *tailExpr = headExpr;
  size_t lastId = endOfCurrBlock(*tf).lastId;
  if(lastId == headToken->id) return (ExprBlock) {headExpr, tailExpr};
  if(nextToken(tf) == NULL) return (ExprBlock) {headExpr, tailExpr};

  ExprBlock tmp;
  do {
    if(currToken(*tf)->info.type == PAR_OPEN) {
      nextToken(tf);
      tmp = createExprBlockTill(tf, PAR_CLOSE);
    }
    else {


      //if the block has a inner block, make a recurse call to it
      if(tf->currElem == 0) {
        /* printf("lastId: %ld, lastLine: %ld\n", endOfCurrBlock(cloneTokenizedFile(*tf)).lastId, endOfCurrBlock(cloneTokenizedFile(*tf)).lastLine); */
        tmp = createExprBlock(tf);
        /* printf("------------------------------------\n"); */
        /* printLinkExprs(tmp.head, 0); */
        /* printf("------------------------------------\n"); */
        while(node_get_neighbour(tmp.tail, RIGHT_LINK)) tmp.tail = node_get_neighbour(tmp.tail, RIGHT_LINK);
        /* printf("here2: %ld\n", currToken(*tf)->id); */
        /* if(nextToken(tf) == NULL) break; */
        /* else returnToken(tf); */
      }
      else { tmp.head = tmp.tail = createExpression(currToken(*tf)); }

    }

    node_set_double_link_at(tailExpr, tmp.head, RIGHT_LINK, LEFT_LINK);
    /* printf("------------------------------------\n"); */
    /* printLinkExprs(headExpr, 0); */
    /* printf("------------------------------------\n"); */
    /* printf("%d %ld %s\n", node_get_num_neighbours(currBlock.tail), expr_node_get_value(currBlock.tail)->id, expr_node_get_value(currBlock.tail)->text); */
    tailExpr = tmp.tail;
    if(currToken(*tf)->id >= lastId) break;
    /* else if(currToken(*tf)->id > lastId) { */
    /*   //print tokenized file informations all */

    /*   exit(1); */
    /* } */
  } while(nextToken(tf));
  Expression *parsedExpr = parseExprLink(headExpr);
  tailExpr = parsedExpr;
  while(node_get_neighbour(tailExpr, RIGHT_LINK)) tailExpr = node_get_neighbour(tailExpr, RIGHT_LINK);
  /* printf("Saindo\n"); */
  /* printLinkExprs(parsedExpr, 0); */
  return (ExprBlock) {parsedExpr, tailExpr};
}

void checkHighLevelBlock(TokenizedFile tf) {
  //some keywords can start a high level block -> fn ...

  Token *tmpTk = currToken(tf), *name;
  switch(tmpTk->info.type) {
    case FUNC:
      tmpTk = nextToken(&tf);
      int argNum = 0;
      if(isType(tmpTk)) {
        name = nextToken(&tf);
        int tmp;
        if(map_get_value(&declaredFuncs, (void **)&name, &tmp)) {
          fprintf(stderr, "Trying to use a function name twice\n");
          exit(1);
        }
        if(name->info.type == NAME_TK) {
          tmpTk = nextToken(&tf);
          if(tmpTk->info.type == COLON) {
            while(nextToken(&tf)) {
              tmpTk = currToken(tf);
              if(isType(tmpTk)) {
                tmpTk = nextToken(&tf);
                if(tmpTk->info.type == NAME_TK) {
                  tmpTk = nextToken(&tf);
                  argNum++;
                  if(tmpTk->info.type == COMMA) continue;
                  else if(tmpTk->info.type == END_BAR) break;
                  else {
                    fprintf(stderr, "Bad function definition, missing end bar delimiter, %d %d\n", tmpTk->l, tmpTk->c);
                    exit(1);
                  }
                } else {
                  fprintf(stderr, "Bad function definition, missing argument name, %d %d\n", tmpTk->l, tmpTk->c);
                  exit(1);
                }
              } else {
                fprintf(stderr, "Bad function definition, missing argument type, %d %d\n", tmpTk->l, tmpTk->c);
                exit(1);
              }
            }
          }
          if(tmpTk->info.type == END_BAR) {
            if(nextToken(&tf) == NULL) {
              fprintf(stderr, "Bad function definition, missing function body, %d %d\n", tmpTk->l, tmpTk->c);
              exit(1);
            }
            map_insert(&declaredFuncs, (void **)&name, (void *)&argNum);
          } else {
            fprintf(stderr, "Bad function definition, missing end bar delimiter, %d %d\n", tmpTk->l, tmpTk->c);
            exit(1);
          }
        } else {
          fprintf(stderr, "Bad function definition, missing function name, %d %d\n", tmpTk->l, tmpTk->c);
          exit(1);
        }

      } else {
        fprintf(stderr, "Bad function definition, missing return type, %d %d\n", tmpTk->l, tmpTk->c);
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "%s must implement it's high level block check\n", tmpTk->text);
      exit(1);
      break;
  }
}

void deleteData(void *data) {
  TokenToParse *casted = (TokenToParse *)data;
  if(casted) {
    free(casted->tk->text);
    free(casted->tk);
    casted->tk = NULL;
  }
}

void destroyExprBlock(ExprBlock *block) {
  if(block == NULL) return;
  node_delete_recursive(block->head, deleteData);
  block->head = block->tail = NULL;
}

ParsedFile createParsedFile(TokenizedFile *tf) {
  if(tf == NULL) return (ParsedFile){0};

  ParsedFile pf = {
    .entryPoint = -1,
    .qtdBlocks = 0,
    .capBlocks = 1,
    .blocks = malloc(sizeof(HighLevelBlock))
  };
  ExprBlock tmpBlock;
  /* printf("line: %d, word: %s\n", currToken(*tf)->l, currToken(*tf)->text); */

  //it will only be used to store the number of arguments a function has, for when parsing, get the right amount
  declaredFuncs = map_create(sizeof(Token *), sizeof(int), cmp_token_to_parse);

  do {
    checkHighLevelBlock(cloneTokenizedFile(*tf));
    tmpBlock = createExprBlock(tf);
    //if it's a function and we didn't find the main yet, check if it's the main function, if it is, set the entry point
    if(pf.entryPoint == -1 && expr_node_get_value(tmpBlock.head).tk->info.type == FUNC) {
      Expression *fnName = node_get_neighbour(node_get_neighbour(tmpBlock.head, CHILD(1)), CHILD(1));
      if(cmpStr(expr_node_get_value(fnName).tk->text, "main"))
        pf.entryPoint = pf.qtdBlocks;
    }
    maybeRealloc((void**)&pf.blocks, (int*)&pf.capBlocks, pf.qtdBlocks, sizeof(HighLevelBlock));
    pf.blocks[pf.qtdBlocks++] = tmpBlock;

    /* printf("is null: %d\n", currToken(*tf) == NULL); */
  } while(nextToken(tf));

  if(pf.entryPoint == -1) {
    fprintf(stderr, "No entry point found\n");
    exit(1);
  }

  map_delete(&declaredFuncs);
  return pf;
}

void destroyParsedFile(ParsedFile *pf) {
  for(int i = 0; i < (int)pf->qtdBlocks; i++)
    destroyExprBlock(&pf->blocks[i]);
  free(pf->blocks);
  pf->blocks = NULL;
  /* printf("destroyed\n"); */
}

#include "parser.h"
#include "node.h"
#include "tokenizer.h"
#include <stdio.h>

INIT_NODE_TYPE(expr, TokenToParse)

Expression *createExpression(Token *tk) {
  TokenToParse tmpToken = {tk, 1 };
  Expression *tmp = expr_node_create(tmpToken);
  node_set_link(tmp, NULL); //parent
  node_set_link(tmp, NULL); //left
  node_set_link(tmp, NULL); //right
  return tmp;
}

ExprBlock createExprBlockTill(TokenizedFile *tf, TokenType endType) {
  ExprBlock currBlock = { createExpression(currToken(*tf)), NULL };
  currBlock.tail = currBlock.head;
  Expression *tmp = NULL;
  nextToken(tf);
  while(currToken(*tf)->typeAndPrecedence.type != endType) {
    tmp = createExpression(currToken(*tf));
    node_set_double_link_at(currBlock.tail, tmp, RIGHT_LINK, LEFT_LINK);
    currBlock.tail = tmp;
    nextToken(tf);
  }
  /* printLinkExprs(currBlock.head, 0); */
  return currBlock;
}

/*
 * Creates a linked list for all the tokens of the block
*/
ExprBlock createExprBlock(TokenizedFile *tf) {
  Token *headToken = currToken(*tf);
  ExprBlock currBlock = {createExpression(headToken), NULL};
  Expression *tmp;
  currBlock.tail = currBlock.head;
  size_t lastId = endOfCurrBlock(*tf).lastId;
  while(1){
    nextToken(tf);
    if(currToken(*tf)->typeAndPrecedence.type == PAR_OPEN) {
      nextToken(tf);
      tmp = parseExprBlock(createExprBlockTill(tf, PAR_CLOSE));
    }
    else {

      /* printf("line: %d, word: %s, ", currToken(*tf)->l, currToken(*tf)->text); */
      /* printf("endOfLine: %ld, currLine: %ld\n", endOfCurrBlock(cloneTokenizedFile(*tf)).lastLine, tf->currLine); */

      //if the block has a inner block, make a recurse call to it
      if(tf->currElem == 0 && (int)endOfCurrBlock(cloneTokenizedFile(*tf)).lastLine != currToken(*tf)->l) {
        /* printf("here: %s\n", currToken(*tf)->text); */
        /* printf("lastId: %ld, lastLine: %ld\n", endOfCurrBlock(cloneTokenizedFile(*tf)).lastId, endOfCurrBlock(cloneTokenizedFile(*tf)).lastLine); */
        ExprBlock tmpBlock = createExprBlock(tf);
        /* printLinkExprs(tmpBlock.head, 0); */
        tmp = parseExprBlock(tmpBlock);
      }
      else
        tmp = createExpression(currToken(*tf));

    }

    node_set_double_link_at(currBlock.tail, tmp, RIGHT_LINK, LEFT_LINK);
    /* printf("%d %ld %s\n", node_get_num_neighbours(currBlock.tail), expr_node_get_value(currBlock.tail)->id, expr_node_get_value(currBlock.tail)->text); */
    currBlock.tail = tmp;
    if(currToken(*tf)->id == lastId) break;
    else if(currToken(*tf)->id > lastId) {
      fprintf(stderr, "You can't do that thing you did");
      exit(1);
    }
  }
  /* printf("here\n"); */
  /* printLinkExprs(currBlock.head, 0); */
  /* printf("here\n"); */
  /* printf("%d %ld %s\n", node_get_num_neighbours(currBlock.tail), expr_node_get_value(currBlock.tail)->id, expr_node_get_value(currBlock.tail)->text); */
  return currBlock;
}

/*
 * Generates the AST of the block
*/
Expression *parseExprBlock(ExprBlock block) {
  Expression *expr = block.head;
  for(int i = BUILTIN_LOW_PREC; i <= BUILTIN_HIGH_PREC; i++) {
    while(node_get_neighbour(expr, PARENT_LINK)) expr = node_get_neighbour(expr, PARENT_LINK);

    Expression *tmpExpr = expr, *right = NULL, *left = NULL;
    /* printf("%s\n", expr_node_get_value(expr).tk->text); */

    TokenToParse leftTk, rightTk;
    Token *tmpToken, *leftToken, *rightToken;
    unsigned char leftIsParsed, rightIsParsed;
    while(node_get_neighbour(tmpExpr, RIGHT_LINK)) {
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

      if(i == tmpToken->typeAndPrecedence.precedence && expr_node_get_value(tmpExpr).toParse) {
        /* printf("entrou: %s prec: %d\n", tmpToken->text, tmpToken->typeAndPrecedence.precedence); */
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

        switch(tmpToken->typeAndPrecedence.precedence) {
          case BUILTIN_LOW_PREC:
            //case for '*' '/' '%'
            // it will only acept neighbours tokens that has the same or lower precedence, or if they
            // are already parsed
            if(leftToken && rightToken) {
              if((!leftIsParsed && leftToken->typeAndPrecedence.precedence > BUILTIN_LOW_PREC)
               ||(!rightIsParsed && rightToken->typeAndPrecedence.precedence > BUILTIN_LOW_PREC)) {
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
            break;
          case BUILTIN_MEDIUM_PREC:
            if(leftToken && rightToken) {
              if((!leftIsParsed && leftToken->typeAndPrecedence.precedence > BUILTIN_MEDIUM_PREC)
               ||(!rightIsParsed && rightToken->typeAndPrecedence.precedence > BUILTIN_MEDIUM_PREC)) {
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
          case BUILTIN_HIGH_PREC:
            switch(tmpToken->typeAndPrecedence.type) {

              case ASSIGN:
              goto getLeftAndRightNeighbours;
                break;
              case FUNC:

              if(right) {
                if(expr_node_get_value(right).tk->typeAndPrecedence.type != NAME_TK) {
                  fprintf(stderr, "%s operation has invalid operands: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                  exit(1);
                }
              } else{
                fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }

              //here we should insert the right token name to a list of user function's
              node_swap_neighbours(tmpExpr, right, RIGHT_LINK, RIGHT_LINK);
              node_remove_link_at(right, RIGHT_LINK); node_remove_link_at(right, LEFT_LINK);
              node_set_double_link_at(tmpExpr, right, CHILD(1), PARENT_LINK);

              //here we should iterate tmpExpr till find the end of function definition, at |
              //storing informations about the type and number of params
                break;

              default:
                break;
            }

            break;

            //unreachble
            case COMPTIME_KNOWN:
            case USER_DEFINITIONS:
              break;

            getLeftAndRightNeighbours:
              node_swap_neighbours(tmpExpr, left, LEFT_LINK, LEFT_LINK);
              node_swap_neighbours(tmpExpr, right, RIGHT_LINK, RIGHT_LINK);
              node_remove_link_at(left, RIGHT_LINK); node_remove_link_at(left, LEFT_LINK);
              node_remove_link_at(right, RIGHT_LINK); node_remove_link_at(right, LEFT_LINK);
              node_set_double_link_at(tmpExpr, left, CHILD(1), PARENT_LINK);
              node_set_double_link_at(tmpExpr, right, CHILD(2), PARENT_LINK);
        }
      }
      while(node_get_neighbour(tmpExpr, PARENT_LINK)) tmpExpr = node_get_neighbour(tmpExpr, PARENT_LINK);

      tmpExpr = node_get_neighbour(tmpExpr, RIGHT_LINK);
      if(tmpExpr == NULL)
        break;
    }
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

  const char *humanReadablePrec[] = {"COMPTIME", "USER_DEF", "LOW_PREC", "BUILTIN_UNARY", "MEDIUM_PREC", "HIGH_PREC"};
  char space[layer+1];
  memset(space, ' ', layer);
  space[layer] = '\0';
  TokenToParse tp = expr_node_get_value(expr);
  printf("%s[text: %s, isParsed: %u, prec: %s]\n", space, tp.tk->text, !tp.toParse, humanReadablePrec[tp.tk->typeAndPrecedence.precedence + 1]);
  for(unsigned i = CHILD(1); i < node_get_num_neighbours(expr); i++) {
    /* printf("%s\n", expr_node_get_value(node_get_neighbour(expr, i))->text); */
    printLinkExprs(node_get_neighbour(expr, i), layer+1);
  }
  printLinkExprs(node_get_neighbour(expr, RIGHT_LINK), layer);
}

void destroyExprBlock(Expression *expr) {
  node_delete_recursive(expr, NULL);
}

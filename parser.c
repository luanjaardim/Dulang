#include "parser.h"
#include "node.h"
#include "tokenizer.h"
#include <stdio.h>

INIT_NODE_TYPE(expr, TokenToParse)

Expression *createExpression(Token *tk) {
  TokenToParse tmpToken = {tk, 1};
  Expression *tmp = expr_node_create(tmpToken);
  node_set_link(tmp, NULL); //parent
  node_set_link(tmp, NULL); //left
  node_set_link(tmp, NULL); //right
  return tmp;
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

    node_set_double_link_at(currBlock.tail, tmp, RIGHT_LINK, LEFT_LINK);
    /* printf("%d %ld %s\n", node_get_num_neighbours(currBlock.tail), expr_node_get_value(currBlock.tail)->id, expr_node_get_value(currBlock.tail)->text); */
    if(currToken(*tf)->id == lastId) break;
    currBlock.tail = tmp;
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
  for(int i = 0; i < HIGH_PRECEDENCE; i++) {

    while(node_get_neighbour(expr, PARENT_LINK)) expr = node_get_neighbour(expr, PARENT_LINK);

    Expression *tmpExpr = expr, *right = NULL, *left = NULL;
    /* printf("%s\n", expr_node_get_value(expr)->text); */

    Token *tmpToken, *leftToken, *rightToken;
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
        left = node_get_neighbour(tmpExpr, LEFT_LINK);
        right = node_get_neighbour(tmpExpr, RIGHT_LINK);

        if(left) leftToken = expr_node_get_value(left).tk;
        else leftToken = NULL;
        if(right) rightToken = expr_node_get_value(right).tk;
        else rightToken = NULL;

        switch(tmpToken->typeAndPrecedence.precedence) {
          case 0:
            if(leftToken && rightToken) {
              if(leftToken->typeAndPrecedence.precedence > 0 && rightToken->typeAndPrecedence.precedence > 0) {
                fprintf(stderr, "%s has invalid args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }
            }
            else {
                fprintf(stderr, "%s insufficient args: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
            }

            goto getLeftAndRightNeighbours;
            break;
          case 1:
            //unary operations, take just one arg after it
            break;
          case 2:
            //case for '*' '/' '%'
              if(expr_node_get_value(left).tk->typeAndPrecedence.precedence > 2 ||
                 expr_node_get_value(right).tk->typeAndPrecedence.precedence >= 2) {
                fprintf(stderr, "%s operation has invalid operands: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }

            goto getLeftAndRightNeighbours;
            break;
          case 3:
            /* printf("3:::: %s prec: %ld\n", tmpToken->text, tmpToken->typeAndPrecedence.precedence); */
            break;
          case 4:
            switch(tmpToken->typeAndPrecedence.type) {

              case ASSIGN:
              goto getLeftAndRightNeighbours;
                break;
              case FUNC:

              if(expr_node_get_value(right).tk->typeAndPrecedence.type != VAR_NAME_TK) {
                fprintf(stderr, "%s operation has invalid operands: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }

              //here we can change the type of the token to FN_NAME
              rightToken->typeAndPrecedence.type = FN_NAME_TK;
              rightToken->typeAndPrecedence.precedence = 3;
              node_swap_neighbours(tmpExpr, right, RIGHT_LINK, RIGHT_LINK);
              node_remove_link_at(right, RIGHT_LINK); node_remove_link_at(right, LEFT_LINK);
              node_set_double_link_at(tmpExpr, right, CHILD(1), PARENT_LINK);
                break;
              default:

              break;
            }
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
  return expr;
}

/* ParsedFile parseTokenizedFile(TokenizedFile tf) { */

/*   return (ParsedFile){0}; */
/* } */

void printLinkExprs(Expression *expr, int layer) {
  if(!expr) return;

  printf("layer: %d, text: %s, num of neighbours %u\n", layer, expr_node_get_value(expr).tk->text, node_get_num_neighbours(expr));
  for(unsigned i = CHILD(1); i < node_get_num_neighbours(expr); i++) {
    /* printf("%s\n", expr_node_get_value(node_get_neighbour(expr, i))->text); */
    printLinkExprs(node_get_neighbour(expr, i), layer+1);
  }
  printLinkExprs(node_get_neighbour(expr, RIGHT_LINK), layer);
}

void destroyExprBlock(Expression *expr) {
  node_delete_recursive(expr, NULL);
}

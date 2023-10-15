#include "parser.h"
#include "node.h"
#include "tokenizer.h"
#include <stdio.h>

INIT_NODE_TYPE(expr, const Token *)

Expression *createExpression(const Token *tk) {
  Expression *tmp = expr_node_create(tk);
  node_set_link(tmp, NULL); //parent
  node_set_link(tmp, NULL); //left
  node_set_link(tmp, NULL); //right
  return tmp;
}

/*
 * Creates a linked list for all the tokens of the block
*/
Expression *createExprBlock(TokenizedFile tf) {
  const Token *headToken = currTokenizedFile(tf);
  Expression *head = createExpression(headToken);
  Expression *tail = head, *tmp;
  size_t last = endOfCurrBlock(tf);
  /* printf("%ld\n", last); */
  while(--last){
    nextTokenizedFile(&tf);
    tmp = createExpression(currTokenizedFile(tf));
    node_set_double_link_at(tail, tmp, RIGHT_LINK, LEFT_LINK);
    /* printf("%d %ld %s\n", node_get_num_neighbours(tail), expr_node_get_value(tail)->id, expr_node_get_value(tail)->text); */
    tail = tmp;
  }
  /* printf("%d %ld %s\n", node_get_num_neighbours(tail), expr_node_get_value(tail)->id, expr_node_get_value(tail)->text); */
  return head;
}

/*
 * Generates the AST of the block
*/
Expression *parseExprBlock(TokenizedFile tf) {
  Expression *expr = createExprBlock(tf);
  for(int i = 0; i < LOW_PRECEDENCE; i++) {

    while(node_get_neighbour(expr, PARENT_LINK)) expr = node_get_neighbour(expr, PARENT_LINK);

    Expression *tmpExpr = expr, *right = NULL, *left = NULL;

    const Token *tmpToken, *leftToken, *rightToken;
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
      tmpToken = expr_node_get_value(tmpExpr);

      if(i == tmpToken->typeAndPrecedence.precedence) {
        left = node_get_neighbour(tmpExpr, LEFT_LINK);
        right = node_get_neighbour(tmpExpr, RIGHT_LINK);

        if(left) leftToken = expr_node_get_value(left);
        else leftToken = NULL;
        if(right) rightToken = expr_node_get_value(right);
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
              /* if()  //should handle inviable cases */
          case 2:
            //case for '*' '/' '%'
              if(expr_node_get_value(left)->typeAndPrecedence.precedence > 2 ||
                 expr_node_get_value(right)->typeAndPrecedence.precedence >= 2) {
                fprintf(stderr, "%s operation has invalid operands: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }

            goto getLeftAndRightNeighbours;
            break;
          case 1:
            //unary operations, take just one arg after it
            break;
          case 3:
            break;
          case 4:
            switch(tmpToken->typeAndPrecedence.type) {

              case ASSIGN:

              goto getLeftAndRightNeighbours;
                break;
              case FUNC:

              if(expr_node_get_value(right)->typeAndPrecedence.type != WORD_TK) {
                fprintf(stderr, "%s operation has invalid operands: %d, %d\n", tmpToken->text, tmpToken->l, tmpToken->c);
                exit(1);
              }

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
  return expr;
}

/* ParsedFile parseTokenizedFile(TokenizedFile tf) { */

/*   return (ParsedFile){0}; */
/* } */

void printLinkExprs(Expression *expr, int layer) {
  if(!expr) return;

  printf("layer: %d, text: %s, number of neighbours %u\n", layer, expr_node_get_value(expr)->text, node_get_num_neighbours(expr));
  for(unsigned i = CHILD(1); i < node_get_num_neighbours(expr); i++) {
    /* printf("%s\n", expr_node_get_value(node_get_neighbour(expr, i))->text); */
    printLinkExprs(node_get_neighbour(expr, i), layer+1);
  }
  printLinkExprs(node_get_neighbour(expr, RIGHT_LINK), layer);
}

void destroyExprBlock(Expression *expr) {
  node_delete_recursive(expr, NULL);
}

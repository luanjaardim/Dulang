#include "generator.h"

TokenToParse get_token_to_parse(Expression *expr) {
    TokenToParse to_ret;
    node_get_value(expr, &to_ret, sizeof(TokenToParse));
    return to_ret;
}

void generateDulangFile(FILE *f, ParsedFile *pf) {
    for(int i = 0; i < pf->qtdBlocks; i++) {
        ExprBlock block = pf->blocks[i];

        Expression *tmp = block.head;
        while(tmp) {
            translateExpression(f, tmp);
            tmp = node_get_neighbour(tmp, RIGHT_LINK);
        }
    }
}

void translateExpression(FILE *f, Expression *expr) {
    TokenType type = get_token_to_parse(expr).tk->typeAndPrecedence.type;
    fprintf(f, ";; -- instruction %ld\n", get_token_to_parse(expr).tk->id);
    switch(type) {
        case INT_TK:
            fprintf(f, "push %d\n", atoi(get_token_to_parse(expr).tk->text));
            break;
        case ASSIGN:

            fprintf(f, ";; -- assign it here\n");
            translateExpression(f, node_get_neighbour(expr, CHILD(2)));
            fprintf(f, ";; -- assign ends here\n");
            break;
        case FUNC:
            /* translateExpression(f, node_get_neighbour(expr, RIGHT_LINK)); */
            printf("Error: function declaration not implemented yet\n");
            break;
        case NUM_ADD:
        case NUM_SUB:
        case NUM_MUL:
        case NUM_DIV:
        case NUM_MOD:
        {
            Expression *left = node_get_neighbour(expr, CHILD(1));
            Expression *right = node_get_neighbour(expr, CHILD(2));
            translateExpression(f, right); //right first because of the stack pop order
            translateExpression(f, left);
            if(get_token_to_parse(left).tk->typeAndPrecedence.type != INT_TK)
                fprintf(f, "pop rax\n");
            else
                fprintf(f, "mov rax, %d\n", atoi(get_token_to_parse(left).tk->text));
            if(get_token_to_parse(right).tk->typeAndPrecedence.type != INT_TK)
                fprintf(f, "pop rbx\n");
            else
                fprintf(f, "mov rbx, %d\n", atoi(get_token_to_parse(right).tk->text));

            switch(type) {
                case NUM_ADD:
                    fprintf(f, "add rax, rbx\n");
                    break;
                case NUM_SUB:
                    fprintf(f, "sub rax, rbx\n");
                    break;
                case NUM_MUL:
                    fprintf(f, "mul rbx\n");
                    break;
                case NUM_DIV:
                    fprintf(f, "div rbx\n");
                    break;
                case NUM_MOD:
                    fprintf(f, "div rbx\n");
                    fprintf(f, "mov rax, rdx\n");
                    break;
                default:
                    printf("Error: unknown token type\n");
                    break;
            }
            fprintf(f, "push rax\n");
        }
            break;

        case NAME_TK:
            break;
        default:
            printf("Error: unknown token type\n");
            break;
    }
}

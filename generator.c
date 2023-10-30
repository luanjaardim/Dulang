#include "generator.h"

TokenToParse get_token_to_parse(Expression *expr) {
    TokenToParse to_ret;
    node_get_value(expr, &to_ret, sizeof(TokenToParse));
    return to_ret;
}

int cmp_token_to_parse(MapPair *f, MapPair *s, size_t key_size) {
    Token *f_tk = *(Token **)(f->key);
    Token *s_tk = *(Token **)(s->key);
    /* pair_get_key(f, (void *)&f_tk, sizeof(TokenToParse)); */
    /* pair_get_key(s, (void *)&s_tk, sizeof(TokenToParse)); */
    printf("inside cmp_token_to_parse\n");
    printf("f: %s\n", f_tk->text);
    printf("s: %s\n", s_tk->text);

    if(f_tk->qtdChars > s_tk->qtdChars) return 1; //f is bigger
    if(f_tk->qtdChars < s_tk->qtdChars) return -1; //s is bigger
    for(int i = 0; i < f_tk->qtdChars; i++) {
        if(f_tk->text[i] != s_tk->text[i]) {
            if(f_tk->text[i] > s_tk->text[i]) return 1; //f is bigger
            else return -1; //s is bigger
        }
    }
    return 0; //they are equal
}

void generateDulangFile(FILE *f, ParsedFile *pf) {
    for(int i = 0; i < pf->qtdBlocks; i++) {
        ExprBlock block = pf->blocks[i];

        Expression *tmp = block.head;
        Map var_map = map_create(sizeof(Token *), sizeof(int), cmp_token_to_parse);
        while(tmp) {
            translateExpression(f, tmp, &var_map);
            tmp = node_get_neighbour(tmp, RIGHT_LINK);
        }
        map_delete(&var_map);
    }
}

int rsp = 0, rbp = 0;

void translateExpression(FILE *f, Expression *expr, Map *var_map) {
    TokenType type = get_token_to_parse(expr).tk->typeAndPrecedence.type;
    fprintf(f, ";; -- instruction %ld\n", get_token_to_parse(expr).tk->id);
    switch(type) {
        case INT_TK:
            fprintf(f, "push %d\n", atoi(get_token_to_parse(expr).tk->text));
            rsp += 8;
            break;
        case ASSIGN:
            translateExpression(f, node_get_neighbour(expr, CHILD(2)), var_map);
            Token *tk = get_token_to_parse(node_get_neighbour(expr, CHILD(1))).tk;
            int tmp, ret;
            ret = map_get_value(var_map, (void **)&tk, (void *)&tmp);

            printf("token: %d\n", ret);
            if(ret) {
                fprintf(f, "pop rax\n");
                rsp -= 8;
                fprintf(f, "mov [rbp-%d], rax\n", tmp);
                //access
            }
            else
                map_insert(var_map, (void **)&tk, &rsp);
                //insert
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
            TokenType left_type = get_token_to_parse(left).tk->typeAndPrecedence.type;
            TokenType right_type = get_token_to_parse(right).tk->typeAndPrecedence.type;

            if(left_type != INT_TK) translateExpression(f, right, var_map); //right first because of the stack pop order
            if(right_type != INT_TK) translateExpression(f, left, var_map);

            if(left_type != INT_TK) {
                fprintf(f, "pop rax\n");
                rsp -= 8;
            }
            else
                fprintf(f, "mov rax, %d\n", atoi(get_token_to_parse(left).tk->text));
            if(right_type != INT_TK) {
                fprintf(f, "pop rbx\n");
                rsp -= 8;
            }
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
            rsp += 8;
        }
            break;

        case NAME_TK:
            break;
        default:
            printf("Error: unknown token type\n");
            break;
    }
}

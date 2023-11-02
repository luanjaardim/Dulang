#include "generator.h"

TokenToParse get_token_to_parse(Expression *expr) {
    TokenToParse to_ret;
    node_get_value(expr, &to_ret, sizeof(TokenToParse));
    return to_ret;
}

int cmp_token_to_parse(MapPair *f, MapPair *s, size_t key_size) {
    (void)key_size;
    Token *f_tk = *(Token **)(f->key);
    Token *s_tk = *(Token **)(s->key);
    /* pair_get_key(f, (void *)&f_tk, sizeof(TokenToParse)); */
    /* pair_get_key(s, (void *)&s_tk, sizeof(TokenToParse)); */
    printf("inside cmp_token_to_parse\n");
    printf("f: %s\n", f_tk->text);
    printf("s: %s\n", s_tk->text);

    if(f_tk->qtdChars > s_tk->qtdChars) return 1; //f is bigger
    if(f_tk->qtdChars < s_tk->qtdChars) return -1; //s is bigger
    for(int i = 0; i < (int)f_tk->qtdChars; i++) {
        if(f_tk->text[i] != s_tk->text[i]) {
            if(f_tk->text[i] > s_tk->text[i]) return 1; //f is bigger
            else return -1; //s is bigger
        }
    }
    return 0; //they are equal
}

//This will store every declaration that requires the .data segment to be appended to the .asm file at the end
char *dataSegment = NULL;
int dataSegmentSize = 0, dataSegmentCap = 0;
/* char *bssSegment = NULL; */

void generateDulangFile(FILE *f, ParsedFile *pf) {
    dataSegment = malloc(sizeof(char) * 100);
    dataSegmentCap = 100;
    char *text= "segment .data\n";
    dataSegmentSize = strlen(text);
    memcpy(dataSegment, text, dataSegmentSize);

    /* insertIntToStr(f); */
    fprintf(f, "segment .text\n");
    fprintf(f, "global _start\n");
    fprintf(f, "_start:\n");
    fprintf(f, "mov rbp, rsp\n");

    for(int i = 0; i < (int)pf->qtdBlocks; i++) {
        ExprBlock block = pf->blocks[i];

        Expression *tmp = block.head;
        Map var_map = map_create(sizeof(Token *), sizeof(int), cmp_token_to_parse);
        while(tmp) {
            translateExpression(f, tmp, &var_map);
            tmp = node_get_neighbour(tmp, RIGHT_LINK);
        }
        map_delete(&var_map);
    }
    /* fprintf(f, "    call int_to_str\n"); */

    fprintf(f, ";;--end of execution, return 0\n");
    fprintf(f, "    mov rdi, 0\n");
    fprintf(f, "    mov rax, 0x3c\n");
    fprintf(f, "    syscall\n");

    fwrite(dataSegment, sizeof(char), dataSegmentSize, f);
    free(dataSegment);
}

int rsp = 0, rbp = 0;

void translateExpression(FILE *f, Expression *expr, Map *var_map) {
    TokenType type = get_token_to_parse(expr).tk->typeAndPrecedence.type;
    fprintf(f, ";; -- instruction %ld\n", get_token_to_parse(expr).tk->id);
    switch(type) {
        case STR_TK:
        {
            Token *tk = get_token_to_parse(expr).tk;
            /*
             * The structure of a string in the data segment is:
             * str_<id>:db"<string>",10,0(\n implicit) (TODO: REMOVE ,10)
             */
            int possibleSizeToAddToDataSegment =
                tk->qtdChars //size of the string
                + 4 //null terminator ('0'), new_line=10(TODO: REMOVE IT) and the '\n' of the line itself
                + 9 // undescore, "str", 2 colon(TODO: REMOVE THE SECOND ONE), "db" and comma
                + 16; //a suposition of the maximum len of the id
            char tmp[tk->qtdChars+1];
            memcpy(tmp, tk->text, tk->qtdChars); //copy the string without the quotes
            tmp[tk->qtdChars] = 0;
            maybeRealloc((void **)&dataSegment, &dataSegmentCap, dataSegmentSize+possibleSizeToAddToDataSegment, sizeof(char));
            dataSegmentSize += sprintf(dataSegment+dataSegmentSize, "str_%ld:db%s,10,0\n", tk->id, tmp);
            fprintf(f, "push str_%ld\n", tk->id);
            rsp += 8;
            break;
        }
        case INT_TK:
            fprintf(f, "push %d\n", atoi(get_token_to_parse(expr).tk->text));
            rsp += 8;
            break;
        case ASSIGN:
            translateExpression(f, node_get_neighbour(expr, CHILD(2)), var_map);
            Token *tk = get_token_to_parse(node_get_neighbour(expr, CHILD(1))).tk;
            int tmp, ret;
            ret = map_get_value(var_map, (void **)&tk, (void *)&tmp);

            /* printf("token: %d\n", ret); */
            if(ret) {
                fprintf(f, "pop rax\n");
                rsp -= 8;
                fprintf(f, "mov [rbp-%d], rax\n", tmp);
            }
            else
                map_insert(var_map, (void **)&tk, (void *)&rsp);
            break;
        case SYSCALL_TK:
            if(node_get_num_neighbours(expr) == CHILD(1)) {
               printf("Empty syscall\n");
               break;
            }
            char *registers[] = {"rax", "rdi", "rsi", "rdx", "r10", "r8", "r9"};
            for(int i = CHILD(1); i < (int)node_get_num_neighbours(expr); i++) {
                translateExpression(f, node_get_neighbour(expr, i), var_map);
                /* printf("%d %d\n", i, CHILD(1)); */
                fprintf(f, "pop %s\n", registers[i-(CHILD(1))]);
                rsp -= 8;
            }
            fprintf(f, "syscall\n");
            break;
        case FUNC:
            printf("%d\n", node_get_num_neighbours(expr));
            translateExpression(f, node_get_neighbour(expr, CHILD(2)), var_map);
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

            if(left_type != INT_TK) translateExpression(f, left, var_map); //right first because of the stack pop order
            if(right_type != INT_TK) translateExpression(f, right, var_map);

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
        {
            Token *tk = get_token_to_parse(expr).tk;
            int tmp, ret;
            ret = map_get_value(var_map, (void **)&tk, (void *)&tmp);
            if(ret) {
                fprintf(f, "push qword[rbp-%d]\n", tmp); //TODO: the size can be variable
                rsp += 8;
            }
            else {
                printf("Error: variable not declared\n");
                exit(1);
            }
            break;
        }
        default:
            printf("Error: unknown token type\n");
            break;
    }
    Expression *right = node_get_neighbour(expr, RIGHT_LINK);
    if(right) translateExpression(f, right, var_map);
}

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
    /* printf("inside cmp_token_to_parse\n"); */
    /* printf("f: %s\n", f_tk->text); */
    /* printf("s: %s\n", s_tk->text); */

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

    insertIntToStr(f);
    fprintf(f, "segment .text\n");
    fprintf(f, "global _start\n");
    fprintf(f, "_start:\n");
    fprintf(f, "mov rbp, rsp\n");

    for(int i = 0; i < (int)pf->qtdBlocks; i++) {
        ExprBlock block = pf->blocks[i];

        Expression *tmp = block.head;
        Map var_map = map_create(sizeof(Token *), sizeof(int), cmp_token_to_parse);
        while(tmp) {
            translateExpression(f, tmp, &var_map, -1);
            tmp = node_get_neighbour(tmp, RIGHT_LINK);
        }
        map_delete(&var_map);
    }

    fprintf(f, ";;--end of execution, return 0\n");
    fprintf(f, "    mov rdi, 0\n");
    fprintf(f, "    mov rax, 0x3c\n");
    fprintf(f, "    syscall\n");

    fwrite(dataSegment, sizeof(char), dataSegmentSize, f);
    free(dataSegment);
}

int rsp = 0, rbp = 0;
int conditionals = -1;

void getConditionAndBody(FILE *f, Expression *expr, Map *var_map, int *currConditional) {
    Expression *condition = node_get_neighbour(expr, CHILD(1)), *body = node_get_neighbour(expr, CHILD(2));
    Token *currToken = get_token_to_parse(expr).tk;
    if(condition == NULL || body == NULL) {
        fprintf(stderr, "Error: not enough arguments for if\n");
        exit(1);
    }
    Expression *right = node_get_neighbour(expr, RIGHT_LINK);
    Token *rightTk = NULL;
    if(right) rightTk = get_token_to_parse(right).tk;
    if(right && currToken->typeAndPrecedence.type == IF_TK && rightTk->typeAndPrecedence.type == ELSE_TK) {
        *currConditional = ++conditionals;
    }

    translateExpression(f, condition, var_map, *currConditional);
    fprintf(f, "pop rax\n");
    rsp -= 8;
    fprintf(f, "cmp rax, 0\n");

    /*
        * TODO: Someday, try to improve the logic of the code below
        *
        * General explain:
        *   currConditional was the way found to have a sequence of if, else if and else inside
        *   another sequence of if, else if and else. As we using a end_cond label to where is the
        *   end of the entire sequence, when a inner sequence exists it messed up the logic of the
        *   end_cond label. To solve this, we needed to pass a local currConditional that changes only
        *   when a new sequence of if, else if and else is found. Maybe the code below can be improved,
        *   but it's working for now.
        */

    if(right == NULL || rightTk->typeAndPrecedence.type != ELSE_TK) {
        //here we are a solo if, we don't need to create a new end_cond, only the local end_if is fine
        if(currToken->typeAndPrecedence.type == IF_TK) {
            fprintf(f, "je .end_if_%ld\n", currToken->id);
            translateExpression(f, body, var_map, *currConditional);
            fprintf(f, ".end_if_%ld:\n", currToken->id);
        }
        //here we are the last one of a sequence that ends without a proper else
        else {
            fprintf(f, "je .end_cond_%d\n", *currConditional);
            translateExpression(f, body, var_map, *currConditional);
        }
    }
    //here we are inside a sequence of if, else if and else, we need a local end and the end of the sequence
    else {
        fprintf(f, "je .end_if_%ld\n", currToken->id);
        translateExpression(f, body, var_map, *currConditional);
        fprintf(f, "jmp .end_cond_%d\n", *currConditional);
        fprintf(f, ".end_if_%ld:\n", currToken->id);
    }
}

void translateExpression(FILE *f, Expression *expr, Map *var_map, int currConditional) {
    TokenType type = get_token_to_parse(expr).tk->typeAndPrecedence.type;
    /* fprintf(f, ";; -- instruction %ld\n", get_token_to_parse(expr).tk->id); */
    switch(type) {
        case STR_TK:
        {
            fprintf(f, ";; -- string %ld\n", get_token_to_parse(expr).tk->id);
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
            fprintf(f, ";; -- int %ld\n", get_token_to_parse(expr).tk->id);
            fprintf(f, "push %d\n", atoi(get_token_to_parse(expr).tk->text));
            rsp += 8;
            break;
        case ASSIGN:
            fprintf(f, ";; -- assign %ld\n", get_token_to_parse(expr).tk->id);
            translateExpression(f, node_get_neighbour(expr, CHILD(2)), var_map, currConditional);
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
            fprintf(f, ";; -- syscall %ld\n", get_token_to_parse(expr).tk->id);
            if(node_get_num_neighbours(expr) == CHILD(1)) {
               printf("Empty syscall\n");
               break;
            }
            int i;
            char *registers[] = {"rax", "rdi", "rsi", "rdx", "r10", "r8", "r9"};

            for(i = CHILD(1); i < (int)node_get_num_neighbours(expr); i++) {
                translateExpression(f, node_get_neighbour(expr, i), var_map, currConditional);
            }
            for( i-= 1; i >= CHILD(1); i--){
                fprintf(f, "pop %s\n", registers[i-(CHILD(1))]);
                rsp -= 8;
            }
            fprintf(f, "syscall\n");
            break;
        case FUNC:
            /* printf("%d\n", node_get_num_neighbours(expr)); */
            translateExpression(f, node_get_neighbour(expr, CHILD(2)), var_map, currConditional);
            printf("Error: function declaration not implemented yet\n");
            break;
        case NUM_ADD:
        case NUM_SUB:
        case NUM_MUL:
        case NUM_DIV:
        case NUM_MOD:
        {
            fprintf(f, ";; -- operation %s\n", get_token_to_parse(expr).tk->text);
            Expression *left = node_get_neighbour(expr, CHILD(1));
            Expression *right = node_get_neighbour(expr, CHILD(2));
            TokenType left_type = get_token_to_parse(left).tk->typeAndPrecedence.type;
            TokenType right_type = get_token_to_parse(right).tk->typeAndPrecedence.type;

            if(left_type != INT_TK) translateExpression(f, left, var_map, currConditional); //right first because of the stack pop order
            if(right_type != INT_TK) translateExpression(f, right, var_map, currConditional);

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
        case IF_TK:
            fprintf(f, ";; -- if %ld\n", get_token_to_parse(expr).tk->id);
            getConditionAndBody(f, expr, var_map, &currConditional);
            break;
        case ELSE_TK:
            if(get_token_to_parse(node_get_neighbour(expr, LEFT_LINK)).tk->typeAndPrecedence.type != ELSE_TK
               && get_token_to_parse(node_get_neighbour(expr, LEFT_LINK)).tk->typeAndPrecedence.type != IF_TK) {
                fprintf(stderr, "Error: else without if\n");
                exit(1);
            }
            //only an else, without if at right
            if(node_get_num_neighbours(expr) == CHILD(2)) {
                fprintf(f, ";; -- else %ld\n", get_token_to_parse(expr).tk->id);
                Expression *child = node_get_neighbour(expr, CHILD(1));
                if(child == NULL) {
                    fprintf(stderr, "Error: else without body\n");
                    exit(1);
                }
                translateExpression(f, child, var_map, currConditional);
                fprintf(f, ".end_cond_%d:\n", currConditional);
            }
            //this is an else if
            else {
                fprintf(f, ";; -- else if %ld\n", get_token_to_parse(expr).tk->id);
                getConditionAndBody(f, expr, var_map, &currConditional);
                Expression *right = node_get_neighbour(expr, RIGHT_LINK);
                //if there is not an else after this else if, we need to put the end of the if here
                if(right == NULL || get_token_to_parse(right).tk->typeAndPrecedence.type != ELSE_TK) {
                    fprintf(f, ".end_cond_%d:\n", currConditional);
                }
            }
            break;
        case NAME_TK:
        {
            fprintf(f, ";; -- user variable\n");
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
        case PRINT_INT:
            fprintf(f, ";; -- dump int\n");
            translateExpression(f, node_get_neighbour(expr, CHILD(1)), var_map, currConditional);
            fprintf(f, "call int_to_str\n");
            break;
        default:
            printf("Error: unknown token type\n");
            break;
    }
    Expression *right = node_get_neighbour(expr, RIGHT_LINK);
    if(right) translateExpression(f, right, var_map, currConditional);
}

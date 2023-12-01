#include "generator.h"

TokenToParse get_token_to_parse(Expression *expr) {
    TokenToParse to_ret;
    node_get_value(expr, &to_ret, sizeof(TokenToParse));
    return to_ret;
}

//This will store every declaration that requires the .data segment to be appended to the .asm file at the end
char *dataSegment = NULL;
int dataSegmentSize = 0, dataSegmentCap = 0;
/* char *bssSegment = NULL; */

int rsp = 0;
int conditionals = -1, loops = -1, insideLoop = 0;

void generateDulangFile(FILE *f, ParsedFile *pf) {
    Generator g = {
      .currConditional = 0,
      .currLoop = 0,
      .func_map = NULL,
    };
    dataSegment = malloc(sizeof(char) * 100);
    dataSegmentCap = 100;
    char *text= "segment .data\n";
    dataSegmentSize = strlen(text);
    memcpy(dataSegment, text, dataSegmentSize);

    insertIntToStr(f);
    fprintf(f, "segment .text\n");
    fprintf(f, "global _start\n");
    g.func_map = pf->declaredFuncs;

    for(int i = 0; i < (int)pf->qtdBlocks; i++) {
        rsp = 0;
        ExprBlock block = pf->blocks[i];

        Expression *tmp = block.head;
        Map map = map_create(sizeof(Token *), sizeof(int), cmp_token_to_parse);
        g.var_map = &map;
        while(tmp) {
            translateExpression(f, tmp, g);
            tmp = node_get_neighbour(tmp, RIGHT_LINK);
        }
        map_delete(g.var_map);
    }

    fwrite(dataSegment, sizeof(char), dataSegmentSize, f);
    free(dataSegment);
}

void deinitVariables(FILE *f, Generator g) {
    /* printf("prev_rsp: %d, curr rsp: %d\n", prev_rsp, rsp); */
    MapPair *vars = g.var_map->pairs;
    int i;
    for(i = g.var_map->qtdPairs - 1; i >= 0; i--) {
        if(*(int *)vars[i].value <= g.prev_rsp) break;
        /* printf("pos to deallocate: %d\n", *(int *)vars[i].value); */
        free(vars[i].key);
        free(vars[i].value);
    }
    fprintf(f, "mov rsp, rbp\n");
    fprintf(f, "sub rsp, %d\n", g.prev_rsp);
    g.var_map->qtdPairs = i+1;
    rsp = g.prev_rsp;
}

void getConditionAndBody(FILE *f, Expression *expr, Generator *g) {
    Expression *condition = node_get_neighbour(expr, CHILD(1)), *body = node_get_neighbour(expr, CHILD(2));
    Token *currToken = get_token_to_parse(expr).tk;
    if(condition == NULL || body == NULL) {
        fprintf(stderr, "Error: not enough arguments for if\n");
        exit(1);
    }
    Expression *right = node_get_neighbour(expr, RIGHT_LINK);
    Token *rightTk = NULL;
    if(right) rightTk = get_token_to_parse(right).tk;
    if(right && currToken->info.type == IF_TK && rightTk->info.type == ELSE_TK) {
        g->currConditional = ++conditionals;
    }

    translateExpression(f, condition, *g);
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

    if(right == NULL || rightTk->info.type != ELSE_TK) {
        //here we are a solo if, we don't need to create a new end_cond, only the local end_if is fine
        if(currToken->info.type == IF_TK) {
            fprintf(f, "je .end_if_%ld\n", currToken->id);
            translateExpression(f, body, *g);
            fprintf(f, ".end_if_%ld:\n", currToken->id);
        }
        //here we are the last one of a sequence that ends without a proper else
        else {
            fprintf(f, "je .end_cond_%d\n", g->currConditional);
            translateExpression(f, body, *g);
        }
    }
    //here we are inside a sequence of if, else if and else, we need a local end and the end of the sequence
    else {
        fprintf(f, "je .end_if_%ld\n", currToken->id);
        translateExpression(f, body, *g);
        fprintf(f, "jmp .end_cond_%d\n", g->currConditional);
        fprintf(f, ".end_if_%ld:\n", currToken->id);
    }
}

void translateExpression(FILE *f, Expression *expr, Generator g) {
    TokenType type = get_token_to_parse(expr).tk->info.type;
    int backUpRsp;
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
        case CHAR_TK:
            fprintf(f, ";; -- char %ld\n", get_token_to_parse(expr).tk->id);
            fprintf(f, "push %s\n", get_token_to_parse(expr).tk->text);
            rsp += 8;
            break;
        case INT_TK:
            fprintf(f, ";; -- int %ld\n", get_token_to_parse(expr).tk->id);
            fprintf(f, "push %s\n", get_token_to_parse(expr).tk->text);
            rsp += 8;
            break;
        case NUM_SIGNAL:
            fprintf(f, ";; -- num_signal %ld\n", get_token_to_parse(expr).tk->id);
            Token *numberToken = get_token_to_parse(node_get_neighbour(expr, CHILD(1))).tk;
            fprintf(f, "push -%s\n", numberToken->text);
            rsp += 8;
            break;
        case ASSIGN:
            fprintf(f, ";; -- assign %ld\n", get_token_to_parse(expr).tk->id);
            translateExpression(f, node_get_neighbour(expr, CHILD(2)), g);
            Expression *firstChild = node_get_neighbour(expr, CHILD(1));
            Token *tk = get_token_to_parse(firstChild).tk;
            int tmp, ret;
            ret = map_get_value(g.var_map, (void **)&tk, (void *)&tmp);

            if(ret && tk->info.type == NAME_TK) {
                fprintf(f, "pop rax\n");
                rsp -= 8;
                //it can be '+' for arguments and '-' for local variables
                fprintf(f, "mov [rbp%s%d], rax\n", tmp > 0 ? "-" : "+", abs(tmp));
            } else if(tk->info.type == DEREF_TK) {
                translateExpression(f, node_get_neighbour(firstChild, CHILD(1)), g);
                fprintf(f, "pop rax\n");
                fprintf(f, "pop rbx\n");
                rsp -= 16;
                fprintf(f, "mov [rax], rbx\n");
            }
            else
                map_insert(g.var_map, (void **)&tk, (void *)&rsp);
            g.prev_rsp = rsp;
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
                translateExpression(f, node_get_neighbour(expr, i), g);
            }
            for( i-= 1; i >= CHILD(1); i--){
                fprintf(f, "pop %s\n", registers[i-(CHILD(1))]);
                rsp -= 8;
            }
            fprintf(f, "syscall\n");
            break;
        case FUNC:
        {
            Expression *returnTypeAndName = node_get_neighbour(expr, CHILD(1));
            Token *name, *tokenChild;
            name = get_token_to_parse(node_get_neighbour(returnTypeAndName, CHILD(1))).tk;
            pairFunc tmp;
            if(map_get_value(g.func_map, (void **)&name, (void *)&tmp)) {
                fprintf(f, ";; -- function %s %ld\n", name->text, name->id);
                int isMain = memcmp("main", name->text, 4) == 0;
                /* printf("child: %s\n", name->text); */
                /* printf("isMain: %d\n", isMain); */
                if(isMain)
                    fprintf(f, "_start:\n");
                else {
                    fprintf(f, "func_%s_%ld:\n", name->text, name->id);
                    fprintf(f, "push rbp\n");
                }
                fprintf(f, "mov rbp, rsp\n");
                int argsOffset = -16;
                for(int i = CHILD(2); i < (int)node_get_num_neighbours(expr) - 1; i++) {
                    tokenChild = get_token_to_parse(
                        node_get_neighbour(node_get_neighbour(expr, i),
                        CHILD(1))).tk;
                    map_insert(g.var_map, (void **)&tokenChild, (void *)&argsOffset);
                    /* printf("name and offset: %s %d\n", tokenChild->text, argsOffset); */
                    argsOffset -= 8;
                }
                translateExpression(f, node_get_neighbour(expr, node_get_num_neighbours(expr) - 1), g);

                fprintf(f, ";; -- end_of_curr_function\n");
                fprintf(f, ".end_curr_func:\n");
                if(isMain) {
                    fprintf(f, ";;--end of execution, return 0\n");
                    fprintf(f, "mov rdi, rax\n");
                    fprintf(f, "mov rax, 0x3c\n");
                    fprintf(f, "syscall\n");
                } else {
                    fprintf(f, "mov rsp, rbp\n");
                    fprintf(f, "pop rbp\n");
                    fprintf(f, "ret\n");
                    fprintf(f, ";; -- end of func\n");
                }

            } else {
                fprintf(stderr, "Error: function %s not declared, %d %d\n", name->text, name->l, name->c);
                exit(1);
            }
        }

            break;
        //these are the operations that can be done with only one operand at the right
        case LOG_NOT:
        case BIT_NOT:
        case BACK_TK:
        {
            fprintf(f, ";; -- %s %ld\n", get_token_to_parse(expr).tk->text, get_token_to_parse(expr).tk->id);
            Expression *child = node_get_neighbour(expr, CHILD(1));
            Token *childTk = get_token_to_parse(child).tk;
            if(childTk->info.type != INT_TK) {
                translateExpression(f, child, g);
                fprintf(f, "pop rax\n");
                rsp -= 8;
            }
            else
                fprintf(f, "mov rax, %s\n", childTk->text);
            if(type == BACK_TK)
                    fprintf(f, "jmp .end_curr_func\n");
            else {
                switch(type) {
                    case BIT_NOT:
                        fprintf(f, "not rax\n");
                        break;
                    case LOG_NOT:
                        fprintf(f, "cmp rax, 0\n");
                        fprintf(f, "sete al\n");
                        fprintf(f, "movzx rax, al\n");
                        break;
                    case BACK_TK:
                        fprintf(f, "jmp .end_curr_func\n");
                        break;
                    default:
                        break;
                }
                fprintf(f, "push rax\n");
                rsp += 8;
            }
        }
        break;
        //these are the operations that can be done with 2 operands, one at the left and one at the right
        case NUM_ADD:
        case NUM_SUB:
        case NUM_MUL:
        case NUM_DIV:
        case NUM_MOD:
        case CMP_EQ:
        case CMP_NE:
        case CMP_LT:
        case CMP_GT:
        case CMP_GE:
        case CMP_LE:
        case LOG_OR:
        case LOG_AND:
        case BIT_OR:
        case BIT_AND:
        case SHIFT_L_TK:
        case SHIFT_R_TK:
        {
            fprintf(f, ";; -- operation %s\n", get_token_to_parse(expr).tk->text);
            Expression *left = node_get_neighbour(expr, CHILD(1));
            Expression *right = node_get_neighbour(expr, CHILD(2));
            TokenType left_type = get_token_to_parse(left).tk->info.type;
            TokenType right_type = get_token_to_parse(right).tk->info.type;

            if(right_type != INT_TK) translateExpression(f, right, g);//right first because of the stack pop order
            if(left_type != INT_TK) translateExpression(f, left, g);

            if(left_type != INT_TK) {
                fprintf(f, "pop rax\n");
                rsp -= 8;
            }
            else
                fprintf(f, "mov rax, %s\n", get_token_to_parse(left).tk->text);
            if(right_type != INT_TK) {
                fprintf(f, "pop rbx\n");
                rsp -= 8;
            }
            else
                fprintf(f, "mov rbx, %s\n", get_token_to_parse(right).tk->text);

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
                    fprintf(f, "xor rdx, rdx\n");
                    fprintf(f, "div rbx\n");
                    break;
                case NUM_MOD:
                    fprintf(f, "xor rdx, rdx\n");
                    fprintf(f, "div rbx\n");
                    fprintf(f, "mov rax, rdx\n");
                    break;
                //bitwise operations
                case SHIFT_L_TK:
                case SHIFT_R_TK:
                    fprintf(f, "mov cl, bl\n"); //HERE WE ARE USING CL, CHANGE THIS WHEN REFACTORING REGISTERS USE
                    fprintf(f, "%s rax, cl\n", (type == SHIFT_L_TK) ? "shl" : "shr");
                    break;
                case BIT_OR:
                case BIT_AND:
                    fprintf(f, "%s rax, rbx\n", (BIT_OR == type) ? "or" : "and");
                    break;
                case LOG_OR:
                case LOG_AND:
                    fprintf(f, "cmp rax, 0\n");
                    fprintf(f, "setne al\n");
                    fprintf(f, "cmp rbx, 0\n");
                    fprintf(f, "setne bl\n");
                    fprintf(f, "%s al, bl\n", (LOG_OR == type) ? "or" : "and");
                    fprintf(f, "movzx rax, al\n");
                    break;
                case CMP_NE:
                case CMP_EQ:
                case CMP_LT:
                case CMP_GT:
                case CMP_LE:
                case CMP_GE:
                    fprintf(f, "cmp rax, rbx\n");
                    switch(type) {
                        case CMP_EQ:
                        case CMP_NE:
                            fprintf(f, "%s al\n", (type == CMP_EQ) ? "sete" : "setne"); break;
                        case CMP_GT:
                        case CMP_LT:
                            fprintf(f, "%s al\n", (type == CMP_GT) ? "setg" : "setl"); break;
                        case CMP_GE:
                        case CMP_LE:
                            fprintf(f, "%s al\n", (type == CMP_GE) ? "setge" : "setle"); break;
                        default:
                            //error of unhandled comparison, tell the line and column
                            printf("Error: unhandled comparison: %d, %d\n", get_token_to_parse(expr).tk->l, get_token_to_parse(expr).tk->c);
                            exit(1);
                            break;
                    }
                    fprintf(f, "movzx rax, al\n");
                    break;
                default:
                    printf("Error: unknown token type for generator\n");
                    break;
            }
            fprintf(f, "push rax\n");
            rsp += 8;
        }
            break;
        case IF_TK:
            fprintf(f, ";; -- if %ld\n", get_token_to_parse(expr).tk->id);
            g.prev_rsp = rsp;
            getConditionAndBody(f, expr, &g);
            if(g.prev_rsp != rsp)
                deinitVariables(f, g);
            break;
        case ELSE_TK:
            if(get_token_to_parse(node_get_neighbour(expr, LEFT_LINK)).tk->info.type != ELSE_TK
               && get_token_to_parse(node_get_neighbour(expr, LEFT_LINK)).tk->info.type != IF_TK) {
                fprintf(stderr, "Error: else without if\n");
                exit(1);
            }
            backUpRsp = g.prev_rsp;
            g.prev_rsp = rsp;
            //only an else, without if at right
            if(node_get_num_neighbours(expr) == CHILD(2)) {
                fprintf(f, ";; -- else %ld\n", get_token_to_parse(expr).tk->id);
                Expression *child = node_get_neighbour(expr, CHILD(1));
                if(child == NULL) {
                    fprintf(stderr, "Error: else without body\n");
                    exit(1);
                }
                translateExpression(f, child, g);
                fprintf(f, ".end_cond_%d:\n", g.currConditional);
            }
            //this is an else if
            else {
                fprintf(f, ";; -- else if %ld\n", get_token_to_parse(expr).tk->id);
                getConditionAndBody(f, expr, &g);
                Expression *right = node_get_neighbour(expr, RIGHT_LINK);
                //if there is not an else after this else if, we need to put the end of the if here
                if(right == NULL || get_token_to_parse(right).tk->info.type != ELSE_TK) {
                    fprintf(f, ".end_cond_%d:\n", g.currConditional);
                }
            }
            if(g.prev_rsp != rsp)
                deinitVariables(f, g);
            g.prev_rsp = backUpRsp;
            break;
        case WHILE_TK:
            fprintf(f, ";; -- while %ld\n", get_token_to_parse(expr).tk->id);
            Expression *condition = node_get_neighbour(expr, CHILD(1)), *body = node_get_neighbour(expr, CHILD(2));
            if(condition == NULL || body == NULL) {
                fprintf(stderr, "Error: not enough arguments for while\n");
                exit(1);
            }
            Generator backup = g;
            g.prev_rsp = rsp;
                g.currLoop = ++loops;
                /* printf("begin while. currLoop: %d, line: %d\n", g.currLoop, get_token_to_parse(expr).tk->l); */
                fprintf(f, ".while_%d:\n", g.currLoop);
                fprintf(f, ";; -- condition check\n");
                translateExpression(f, condition, g);
                fprintf(f, "pop rax\n");
                rsp -= 8;
                fprintf(f, "cmp rax, 0\n");
                fprintf(f, "je .end_while_%d\n", g.currLoop);
                insideLoop++;
                translateExpression(f, body, g);
                if(g.prev_rsp != rsp) {
                    fprintf(f, ";; -- while inner deallocation\n");
                    deinitVariables(f, g);
                }
                fprintf(f, "jmp .while_%d\n", g.currLoop);
                fprintf(f, ".end_while_%d:\n", g.currLoop);
            //we need to deallocate here to because the loop can end at any point with a 'stop'
            if(g.prev_rsp != rsp) {
                fprintf(f, ";; -- while outter deallocation\n");
                deinitVariables(f, g);
            }
            g = backup;
            insideLoop--;
            /* printf("end while. currLoop: %d, line: %d\n", g.currLoop, get_token_to_parse(expr).tk->l); */
            break;
        case STOP_TK:
        case SKIP_TK:
            if(insideLoop) {
                fprintf(f, ";; -- %s\n", (type == STOP_TK) ? "stop" : "skip");
                fprintf(f, "jmp .%s_%d\n", (type == STOP_TK) ? "end_while" : "while", g.currLoop);
            } else {
                fprintf(stderr, "Error: %s outside loop: %d, %d\n", get_token_to_parse(expr).tk->text, get_token_to_parse(expr).tk->l, get_token_to_parse(expr).tk->c);
                exit(1);
            }
            break;
        case NAME_TK:
        {
            fprintf(f, ";; -- user variable\n");
            Token *tk = get_token_to_parse(expr).tk;
            if(tk->info.precedence == USER_DEFINITIONS) {
                int tmp;
                if(map_get_value(g.var_map, (void **)&tk, (void *)&tmp)) {
                    //it can be '+' for arguments and '-' for local variables
                    fprintf(f, "push qword[rbp%s%d]\n", tmp > 0 ? "-" : "+", abs(tmp)); //TODO: the size can be variable
                    rsp += 8;
                }
                else {
                    fprintf(stderr, "Error: variable %s not declared, %d %d\n", tk->text, tk->l, tk->c);
                    exit(1);
                }
            } else if(tk->info.precedence == USER_FUNCTIONS) {
                int someThingWasPushed;
                pairFunc tmp;
                if(map_get_value(g.func_map, (void **)&tk, (void *)&tmp)) {
                    backUpRsp = g.prev_rsp;
                    g.prev_rsp = rsp;
                    for(int i = (int)node_get_num_neighbours(expr) - 1; i >= CHILD(1); i--) {
                        someThingWasPushed = rsp;
                        translateExpression(f, node_get_neighbour(expr, i), g);
                        if(someThingWasPushed == rsp) {
                            fprintf(stderr, "Error: function %s requires expressions that return values\n", tk->text);
                            exit(1);
                        }
                    }
                    fprintf(f, ";; -- function call %s\n", tk->text);
                    fprintf(f, "call func_%s_%d\n", tk->text, tmp.id);
                    deinitVariables(f, g);
                    g.prev_rsp = backUpRsp;
                    fprintf(f, "push rax\n");
                    rsp += 8;
                }
            } else {
                fprintf(stderr, "Error: Something very strange has happened, %d %d\n", tk->l, tk->c);
                exit(1);
            }

            break;
        }
        case PRINT_INT:
            fprintf(f, ";; -- dump int\n");
            translateExpression(f, node_get_neighbour(expr, CHILD(1)), g);
            fprintf(f, "call int_to_str\n");
            fprintf(f, "add rsp, 8\n");
            rsp -= 8;
            break;
        case DEREF_TK:
            fprintf(f, ";; -- deref\n");
            translateExpression(f, node_get_neighbour(expr, CHILD(1)), g);
            fprintf(f, "pop rax\n");
            fprintf(f, "push qword[rax]\n");
            break;
        default:
            printf("Error: unknown token type for generator: %s, %d %d\n", get_token_to_parse(expr).tk->text, get_token_to_parse(expr).tk->l, get_token_to_parse(expr).tk->c);
            exit(1);
    }
    Expression *right = node_get_neighbour(expr, RIGHT_LINK);
    if(right) translateExpression(f, right, g);
}

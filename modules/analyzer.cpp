#include "analyzer.h"
#include "utils.h"

DefinitionsHandler defs;

AnalyzedParsedFile *analyzeToken(ParsedFile *tokens);

string typeString(Type t) {
  switch(t.base) {
    case TYPE_FUNC:
    case TYPE_TAG_UNION:
    case TYPE_COMPOUND:
    {
      string symbol[3] = {" -> ", " ^ ", " & " };
      string typeText = "";
      for(int i = 0; i < (int)t.subTypes.size(); i++) {
        Type s = t.subTypes[i];
        typeText += (s.base >= TYPE_FUNC && s.base <= TYPE_COMPOUND ? 
            "(" + typeString(s) + ")" : typeString(s)) + (i != (int)t.subTypes.size() - 1 ? symbol[t.base - TYPE_FUNC] : "");
      }
      return typeText;
    }
    case TYPE_REF:
      {
        Type s = t.subTypes[0];
        if(s.base >= TYPE_FUNC && s.base <= TYPE_COMPOUND)
          return "#(" + typeString(s) + ")";
        return "#" + typeString(s);
      }
    case TYPE_INT:
      return "int";
    case TYPE_BYTE:
      return "byte";
    case TYPE_NONE:
      return "none";
    case TYPE_USER_DEFINED:
      return t.textType;
    default:
      return "unknown";
  }
}

Type analyzeType(ParsedFile *tokens) {
  if(tokens == NULL)
    return Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};

  TokenType type = tokens->get_data()->type;
  if(type == TK_TYPE_FN_ARROW || type == TK_TYPE_TAG_UNION || type == TK_TYPE_COMPOUND) {
    Type firstChild = analyzeType(tokens->get_neighbor(CHILD(1)));
    Type secondChild = analyzeType(tokens->get_neighbor(CHILD(2)));
    string innerText[3] = {" -> ", " ^ ", " & " };
    BaseType types[3] = {TYPE_FUNC, TYPE_TAG_UNION, TYPE_COMPOUND};
    vector<Type> subTypes = {firstChild};
    if(secondChild.base == types[type - TK_TYPE_FN_ARROW])
      subTypes.insert(subTypes.end(), secondChild.subTypes.begin(), secondChild.subTypes.end());
    else
      subTypes.push_back(secondChild);
    return Type{
      .textType = firstChild.textType + innerText[type - TK_TYPE_FN_ARROW] + secondChild.textType,
      .base = types[type - TK_TYPE_FN_ARROW],
      .subTypes = subTypes
    }; 
  } else if(type == TK_TYPE_REF) {
    Type t = analyzeType(tokens->get_neighbor(CHILD(1)));
    return Type{.textType = "#" + t.textType, .base = TYPE_REF, .subTypes = {t}};
  } else if(type == TK_TYPE_INT) {
    return Type{.textType = "int", .base = TYPE_INT, .subTypes = {}};
  } else if(type == TK_TYPE_BYTE) {
    return Type{.textType = "byte", .base = TYPE_BYTE, .subTypes = {}};
  } else if(type == TK_TYPE_NONE) {
    return Type{.textType = "none", .base = TYPE_NONE, .subTypes = {}};
  } else if(type == TK_NAME) {
    return Type{.textType = tokens->get_data()->text, .base = TYPE_USER_DEFINED, .subTypes = {}};
  } else {
    return Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};
  }
}

// must be a name followed by a type as a child, and it will return a Variable
Variable analyzeParseType(ParsedFile *tokens) {
  Token *name = tokens->get_data()->type == TK_TYPE_PARSE ? tokens->get_neighbor(CHILD(1))->get_data() : tokens->get_data();
  return Variable{
    .id = name->id,
    .name = name->text,
    .mut = false,
    .pos = Position(name->l, name->c),
    .type = analyzeType(tokens->get_neighbor(CHILD(2)))
  };
}

AnalyzedParsedFile *analyzeFunc(ParsedFile *tokens) {
  ParsedFile *tmp = tokens;
  OperationFuncDef funcDef;
  Type t;
  t.base = TYPE_FUNC;
  defs.scopes.push_back(Scope(SCOPE_FUNC));      //start of function scope
  while(tmp->get_data()->type != TK_END_BAR && tmp->get_data()->type != TK_FN_RETURN) {
    if(tmp->get_neighbors_size() > CHILD(1)) {
      funcDef.args.push_back(analyzeParseType(tmp->get_neighbor(CHILD(1)))); //get the arguments
      t.subTypes.push_back(funcDef.args.back().type); //get to make the type of the function
    }
    tmp = tmp->get_neighbor(RIGHT_LINK);
  }
  if(t.subTypes.empty()) t.subTypes.push_back(Type{.textType = "none", .base = TYPE_NONE, .subTypes = {}}); //no arguments
  if(tmp->get_data()->type != TK_FN_RETURN)
    t.subTypes.push_back(Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}}); //return type
  else {
    t.subTypes.push_back(analyzeType(tmp->get_neighbor(CHILD(1)))); //return type
    tmp = tmp->get_neighbor(RIGHT_LINK);
  }
  Type *thisFuncVarType = &defs.getLastDefinition()->type;
  //get the function type as text
  t.textType = typeString(t);
  funcDef.type = t;

  if(!confirmType(thisFuncVarType, &(funcDef.type))) {
    printf("Error: type mismatch at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
    printf("Expected: %s, Found: %s\n", thisFuncVarType->textType.c_str(), t.textType.c_str());
    exit(1);
  }
  //possibly updating arguments types
  for(int i = 0; i < (int)funcDef.args.size(); i++) {
    Variable *v = &funcDef.args[i];
    v->type = funcDef.type.subTypes[i]; //update the type of the arguments
    defs.addDefinition(*v); //push the arguments to the scope
  }

  //goes to the operations
  for(int i = CHILD(1); i < (int)tmp->get_neighbors_size(); i++) {
    if(tmp->get_neighbor(i)->get_data() == NULL) continue;
    funcDef.ops.push_back(analyzeParsedFile(tmp->get_neighbor(i)));
  }
  defs.popDefinitions(); //pop the variables from the scope

  return new AnalyzedParsedFile(new Operation(funcDef, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

AnalyzedParsedFile *analyzeTypeDef(ParsedFile *tokens) {
  OperationTypeDef typeDef;
  ParsedFile *tmp = tokens;
  tmp = tmp->get_neighbor(RIGHT_LINK); //go to the name of the new type
  typeDef.var.name = tmp->get_data()->text;
  typeDef.var.id = tmp->get_data()->id;
  typeDef.var.pos = Position(tmp->get_data()->l, tmp->get_data()->l);
  tmp = tmp->get_neighbor(RIGHT_LINK); //go to the end bar
  typeDef.var.type = analyzeType(tmp->get_neighbor(CHILD(1)));

  defs.addDefinition(typeDef.var);
  return new AnalyzedParsedFile(new Operation(typeDef, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

bool confirmType(Type *t, Type *s) {
  if(t->base == TYPE_UNKNOWN || s->base == TYPE_UNKNOWN) {
    *t = s->base == TYPE_UNKNOWN ? *t : *s;
    *s = t->base == TYPE_UNKNOWN ? *s : *t;
    return true;
  }
  else {
    if((t->base == TYPE_TAG_UNION && s->base != TYPE_TAG_UNION) || (t->base != TYPE_TAG_UNION && s->base == TYPE_TAG_UNION)) {
      Type *tagUnion = t->base == TYPE_TAG_UNION ? t : s;
      Type *simpleType = t->base == TYPE_TAG_UNION ? s : t;
      if(simpleType->base != TYPE_UNKNOWN)
        for(int i = 0; i < (int)tagUnion->subTypes.size(); i++) {
          if(confirmType(&tagUnion->subTypes[i], simpleType)) return true;
        }
      return false;
    }
    else if(t->base != s->base) return false; //same base type
    else {
      if(t->subTypes.size() != s->subTypes.size()) return false;
      for(int i = 0; i < (int)t->subTypes.size(); i++) {
        if(!confirmType(&t->subTypes[i], &s->subTypes[i])) return false;
        t->subTypes[i].textType = typeString(t->subTypes[i]);
        s->subTypes[i].textType = typeString(s->subTypes[i]);
      }
    }
  }
  t->textType = typeString(*t);
  s->textType = typeString(*s);
  return true;
}

AnalyzedParsedFile *analyzeVar(ParsedFile *tokens) {
  OperationVarDef varDef;
  ParsedFile *tmp = tokens;
  bool assignVariable = false;
  // cout << "Var: " << tokens->get_data()->text << endl;
  if(tokens->get_data()->type == TK_CONSTANT || tokens->get_data()->type == TK_VARIABLE) {
    //mutable?
    varDef.var.mut = tokens->get_data()->type == TK_VARIABLE;
    //name of the variable
    Token *name = tmp->get_neighbor(CHILD(1))->get_data();
    varDef.var.name = name->text;
    varDef.var.pos = Position(name->l, name->c);
    varDef.var.id = name->id;

    //type of the variable
    tmp = tmp->get_neighbor(RIGHT_LINK);
    if(tmp->get_data()->type == TK_VARIABLE || tmp->get_data()->type == TK_TYPE_PARSE) {
      varDef.var.type = analyzeType(tmp->get_neighbor(CHILD(1)));
      tmp = tmp->get_neighbor(RIGHT_LINK);
    } else {
      varDef.var.type = Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};
    }
    tmp = tmp->get_neighbor(CHILD(1));
  } else if(tokens->get_data()->type == TK_TYPE_PARSE) {
    varDef.var = analyzeParseType(tokens);
    tmp = tmp->get_neighbor(RIGHT_LINK);
    tmp = tmp->get_neighbor(CHILD(1));
  } else if(tokens->get_data()->type == TK_ASSIGN) {
    Token *name = tmp->get_neighbor(CHILD(1))->get_data();
    Variable *v;
    if((v = defs.findDefinition(name->text)) != NULL && v->mut) {
      varDef.var = *v;
      assignVariable = true;
    } else {
      varDef.var.mut = false;
      varDef.var.name = name->text;
      varDef.var.pos = Position(name->l, name->c);
      varDef.var.id = name->id;
      varDef.var.type = Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};
    }
    tmp = tmp->get_neighbor(CHILD(2));
  }

  if(tmp->get_data()->type == TK_BLOCK_FUNC) {
    //with functions we push the variable first, so inside the function we can use it to infer types
    if(varDef.var.mut) {
      printf("You can't define a mutable function. At line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
      exit(1);
    }
    defs.addDefinition(varDef.var);
    varDef.value = analyzeParsedFile(tmp);//goes to the value of the variable
    varDef.var.type = varDef.value->get_data()->funcDef.type;
  } else {
    varDef.value = analyzeParsedFile(tmp);//goes to the value of the variable
    Type *valueType;
    switch(varDef.value->get_data()->type) {
      case OP_TOKEN:
        valueType = &(varDef.value->get_data()->tk.type); break;
      case OP_FUNC_CALL:
        valueType = &(varDef.value->get_data()->funcCall.returnType); break;
      default:
        printf("Error: expected a token, function call or function definition at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
        exit(1);
    }

    //confront variable type with value type
    if(!confirmType(&varDef.var.type, valueType)) {
      printf("Error: type mismatch at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
      printf("Expected: %s, Found: %s\n", varDef.var.type.textType.c_str(), valueType->textType.c_str());
      exit(1);
    }
    if(!assignVariable)
      defs.addDefinition(varDef.var); //push the variable to the scope
  }

  return new AnalyzedParsedFile(
            new Operation(varDef, Position(tokens->get_data()->l, tokens->get_data()->c))
  );
}

AnalyzedParsedFile *analyzeCond(ParsedFile *tokens) {
  OperationCond::CondType condType = OperationCond::NONE;
  ParsedFile *tmp = tokens;
  OperationCond cond;
  if(tmp->get_data()->type == TK_BLOCK_ELSE) {
    ParsedFile *prevBrother = tokens->get_prev_brother();
    if(!prevBrother || (prevBrother->get_data()->type != TK_BLOCK_IF && prevBrother->get_data()->type != TK_BLOCK_ELSE)) {
      printf("Error: else without if at line: %d, column: %d\n", (int)tmp->get_data()->l, (int)tmp->get_data()->c);
      exit(1);
    }
    //goes to possible 'if' or to the '|'
    tmp = tmp->get_neighbor(RIGHT_LINK);
    cond.expr = NULL;
    condType = OperationCond::ELSE;
  }
  if(tmp->get_data()->type == TK_BLOCK_IF ) {
    ParsedFile *child = tmp->get_neighbor(CHILD(1));
    cond.expr = analyzeParsedFile(child);
    //goes to the operations
    tmp = tmp->get_neighbor(RIGHT_LINK);
    condType = condType == 0 ? OperationCond::IF : OperationCond::ELSE_IF;
  }
  cond.type = condType;

  //get body
  defs.scopes.push_back(Scope(SCOPE_COND));
  for(int i = CHILD(1); i < (int)tmp->get_neighbors_size(); i++) {
    if(tmp->get_neighbor(i) == NULL) continue;
    cond.ops.push_back(analyzeParsedFile(tmp->get_neighbor(i)));
  }
  defs.popDefinitions(); //pop the variables from the scope

  return new AnalyzedParsedFile(new Operation(cond, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

AnalyzedParsedFile *analyzeLoop(ParsedFile *tokens) {
  ParsedFile *tmp = tokens;
  OperationLoop loop;
  if(tmp->get_data()->type == TK_BLOCK_WHILE) {
    loop.expr = analyzeToken(tmp->get_neighbor(CHILD(1)));
    //goes to the operations
    tmp = tmp->get_neighbor(RIGHT_LINK);
  }
  //get body
  defs.scopes.push_back(Scope(SCOPE_LOOP));
  for(int i = CHILD(1); i < (int)tmp->get_neighbors_size(); i++) {
    if(tmp->get_neighbor(i)->get_data() == NULL) continue;
    loop.ops.push_back(analyzeParsedFile(tmp->get_neighbor(i)));
  }
  defs.popDefinitions(); //pop the variables from the scope

  return new AnalyzedParsedFile(new Operation(loop, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

AnalyzedParsedFile *analyzeFuncCall(ParsedFile *tokens) {
  AnalyzedParsedFile *node = analyzeToken(tokens);
  if(node->get_data()->type != OP_TOKEN) { //this is not an error, but at the moment we only accept tokens
    printf("Analyzer Error: expected a token at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
    exit(1);
  }
  OperationToken *op = &node->get_data()->tk;
  Type *t = &op->type;
  if(op->type.base != TYPE_FUNC) {
    printf("Error: Expected a function at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
    exit(1);
  }
  OperationFuncCall funcCall;
  funcCall.func = node;
  ParsedFile *tmp = tokens;
  tmp = tmp->get_neighbor(RIGHT_LINK);
  if(tmp->get_neighbors_size() > CHILD(1)) {
    size_t i = 0;
    while(tmp->get_data()->type != TK_ROU_BRA_CLOSE) {
      if(i == t->subTypes.size() - 1) {
        printf("Error: Too many arguments at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
        exit(1);
      }
      node = analyzeParsedFile(tmp->get_neighbor(CHILD(1)));
      // WARN: the code bellow can cause bugs, maybe
      Type *s = node->get_data()->type == OP_TOKEN ? &node->get_data()->tk.type : &node->get_data()->funcCall.returnType;
      if(!confirmType(&t->subTypes[i], s)) {
        printf("Error: type mismatch at line: %d, column: %d\n", (int)tmp->get_data()->l, (int)tmp->get_data()->c);
        printf("Expected: %s, Found: %s\n", t->subTypes[i].textType.c_str(), s->textType.c_str());
        exit(1);
      }
      funcCall.params.push_back(node);
      i++;
      tmp = tmp->get_neighbor(RIGHT_LINK);
    }
    if(i < t->subTypes.size() - 1) {
      funcCall.returnType.base = TYPE_FUNC;
      funcCall.returnType.subTypes = vector(t->subTypes.begin() + i, t->subTypes.end());
    } else
      funcCall.returnType = t->subTypes[i];
  } else { //there is no arguments passed
    if(t->subTypes[0].base != TYPE_NONE) {
      printf("Error: received \"none\" when expecting \"%s\" at line: %d, column: %d\n", typeString(t->subTypes[0]).c_str(), (int)tokens->get_data()->l, (int)tokens->get_data()->c);
      exit(1);
    }
    funcCall.returnType = t->subTypes[t->subTypes.size() - 1];
  }
  funcCall.returnType.textType = typeString(funcCall.returnType);
  return new AnalyzedParsedFile(new Operation(funcCall, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

AnalyzedParsedFile *analyzeToken(ParsedFile *tokens) {
  Type t;
  switch(tokens->get_data()->type) {
    case TK_TYPE_PARSE:
    {
      t = analyzeType(tokens->get_neighbor(CHILD(2)));
      tokens = tokens->get_neighbor(CHILD(1));
    }
    break;
    case TK_INT:
      { t = Type{.textType = "int", .base = TYPE_INT, .subTypes = {}}; }
    break;
    case TK_STR:
    { 
      t = Type{.textType = "#byte", .base = TYPE_REF, .subTypes = {Type{
        .textType = "byte", .base = TYPE_BYTE, .subTypes = {}
      }}}; 
    }
    break;
    case TK_CHAR:
      { t = Type{.textType = "byte", .base = TYPE_BYTE, .subTypes = {}}; }
    break;
    case TK_NAME:
      {
        Variable *v = defs.findDefinition(tokens->get_data()->text);
        if(v == NULL) {
          printf("Error: Expected an already defined variable at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
          exit(1);
        }
        return new AnalyzedParsedFile(
          new Operation(OperationToken{.tk = tokens->get_data(), .type = v->type}, Position(tokens->get_data()->l, tokens->get_data()->c))
        );
      }
    break;
    case TK_TYPE_DEREF:
    case TK_TYPE_REF:
      {
        AnalyzedParsedFile *child = analyzeParsedFile(tokens->get_neighbor(CHILD(1)));
        if(child->get_data()->type != OP_TOKEN) {
          printf("Error: expected a token at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
          exit(1);
        }
        if(tokens->get_data()->type == TK_TYPE_REF) {
          t = Type{.textType = "", .base = TYPE_REF, .subTypes = {child->get_data()->tk.type}};
          t.textType = typeString(t);
        }
        else if(tokens->get_data()->type == TK_TYPE_DEREF) {
          if(child->get_data()->tk.type.base != TYPE_REF) {
            printf("Error: expected a reference type at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
            exit(1);
          }
          t = child->get_data()->tk.type.subTypes[0];
        }
        AnalyzedParsedFile *node = new AnalyzedParsedFile(
          new Operation(OperationToken{.tk = tokens->get_data(), .type = t}, Position(tokens->get_data()->l, tokens->get_data()->c))
        );
        AnalyzedParsedFile::linkFatherAndChild(node, child);
        if(tokens->get_neighbors_size() > CHILD(2))// index dereference
          AnalyzedParsedFile::linkFatherAndChild(node, analyzeParsedFile(tokens->get_neighbor(CHILD(2))));
        return node;
      }
    break;
    default:
      //operations
      if(tokens->get_data()->type >= TK_NUM_ADD
        && tokens->get_data()->type <= TK_BIT_XOR)
      {
        AnalyzedParsedFile *left = analyzeParsedFile(tokens->get_neighbor(CHILD(1)));
        AnalyzedParsedFile *right = analyzeParsedFile(tokens->get_neighbor(CHILD(2)));
          if(left->get_data()->type != OP_TOKEN || right->get_data()->type != OP_TOKEN || left->get_data()->type != right->get_data()->type) {
          printf("Error: expected two tokens with the same type at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
          exit(1);
        } else {
          t = left->get_data()->tk.type;
          OperationToken tk = OperationToken{.tk = tokens->get_data(), .type = t};
          AnalyzedParsedFile *node = new AnalyzedParsedFile(
            new Operation(tk, Position(tokens->get_data()->l, tokens->get_data()->c))
          );
          AnalyzedParsedFile::linkFatherAndChild(node, left);
          AnalyzedParsedFile::linkFatherAndChild(node, right);
          return node;
        }
      }
      { t = Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}}; }
  }
  OperationToken tk = OperationToken{.tk = tokens->get_data(), .type = t};
  AnalyzedParsedFile *node = new AnalyzedParsedFile(
    new Operation(tk, Position(tokens->get_data()->l, tokens->get_data()->c))
  );
  for(int i = CHILD(1); i < (int)tokens->get_neighbors_size(); i++) {
    if(tokens->get_neighbor(i)->get_data() == NULL) continue;
    AnalyzedParsedFile::linkFatherAndChild(node, analyzeParsedFile(tokens->get_neighbor(i)));
  }
  if(tokens->get_neighbor(RIGHT_LINK) != NULL)
    AnalyzedParsedFile::linkNodeNextTo(node, analyzeParsedFile(tokens->get_neighbor(RIGHT_LINK)));
  return node;
}

AnalyzedParsedFile *analyzeParsedFile(ParsedFile *tokens) {
  switch(tokens->get_data()->type) {
    case TK_BLOCK_FUNC:
      return analyzeFunc(tokens);
    case TK_BLOCK_TYPE:
      return analyzeTypeDef(tokens);
    case TK_VARIABLE:
    case TK_CONSTANT:
    case TK_ASSIGN:
      return analyzeVar(tokens);
    case TK_TYPE_PARSE:
      if(tokens->get_neighbor(RIGHT_LINK) && tokens->get_neighbor(RIGHT_LINK)->get_data()->type == TK_ASSIGN)
        return analyzeVar(tokens);
      return analyzeToken(tokens);
    case TK_BLOCK_IF:
    case TK_BLOCK_ELSE:
      return analyzeCond(tokens);
    case TK_BLOCK_WHILE:
      return analyzeLoop(tokens);
    case TK_NAME:
    case TK_TYPE_DEREF:
      if(tokens->get_neighbor(RIGHT_LINK) && tokens->get_neighbor(RIGHT_LINK)->get_data()->type == TK_ROU_BRA_OPEN)
        return analyzeFuncCall(tokens);
      return analyzeToken(tokens);
    default:
      return analyzeToken(tokens);
  }
  return NULL;
}

void printAnalyzerParsedFile(AnalyzedParsedFile *parsedFile, string tab) {
  if(parsedFile == NULL) return;
  Operation *op = parsedFile->get_data();
  switch(op->type) {
    case OP_TOKEN:
      cout << tab << "Token: " << op->tk.tk->text << " Type: " << op->tk.type.textType << endl;
      for(int i = CHILD(1); i < (int)parsedFile->get_neighbors_size(); i++) {
        if(parsedFile->get_neighbor(i) == NULL) continue;
        printAnalyzerParsedFile(parsedFile->get_neighbor(i), tab+"  ");
      }
      if(parsedFile->get_neighbor(RIGHT_LINK) != NULL)
        printAnalyzerParsedFile(parsedFile->get_neighbor(RIGHT_LINK), tab);
      break;
    case OP_FUNC_DEF:
      cout << tab << "Function def, Type: " << op->funcDef.type.textType << endl;
      for(auto arg : op->funcDef.args) {
        cout << tab+"  " << "Arg: " << arg.name << " Type: " << arg.type.textType << endl;
      }
      cout << tab << "Operations: " << endl;
      for(auto o : op->funcDef.ops) {
        printAnalyzerParsedFile(o, tab+"  ");
      }
      break;
    case OP_VAR_DEF:
      {
        if(op->varDef.var.mut)
          cout << tab << "Variable def: " << op->varDef.var.name << " Type: " << op->varDef.var.type.textType << endl;
        else
          cout << tab << "Constant def: " << op->varDef.var.name << " Type: " << op->varDef.var.type.textType << endl;
        printAnalyzerParsedFile(op->varDef.value, tab+"  ");
        break;
      }
    case OP_COND:
      {
        vector<string> condTypes = {"NONE", "IF", "ELSE", "ELSE_IF"};
        cout << tab << "Cond: " << condTypes[op->cond.type] << endl;
        cout << tab+"  " << "Expr: " << endl;
        printAnalyzerParsedFile(op->cond.expr, tab+"    ");
        cout << tab+"  " << "Operations: " << endl;
        for(auto o : op->cond.ops) {
          printAnalyzerParsedFile(o, tab+"    ");
        }
      }
      break;
    case OP_LOOP:
      {
        cout << tab << "Loop: " << endl;
        printAnalyzerParsedFile(op->loop.expr, tab+"  ");
        cout << tab+"  " << "Operations: " << endl;
        for(auto o : op->loop.ops) {
          printAnalyzerParsedFile(o, tab+"    ");
        }
        break;
      }
    case OP_TYPE_DEF:
      {
        OperationTypeDef typeDef = op->typeDef;
        cout << tab << "New type defined: " << typeDef.var.name << " alias of: " << typeDef.var.type.textType << endl;
        break;
      }
    case OP_FUNC_CALL:
      {
        OperationFuncCall funcCall = op->funcCall;
        cout << tab << "Function call: " << endl;
        printAnalyzerParsedFile(funcCall.func, tab+"  ");
        cout << tab+"  " << "Params: " << endl;
        for(auto p : funcCall.params) {
          printAnalyzerParsedFile(p, tab+"    ");
        }
        break;
      }
    default:
      printf("not implemented yet\n");
      exit(1);
  }
}

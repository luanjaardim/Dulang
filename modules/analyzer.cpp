#include "analyzer.h"
#include "utils.h"

DefinitionsHandler defs;
Position confirmTypeErrPos;

AnalyzedParsedFile *analyzeToken(ParsedFile *tokens);

Type getTypeFromAnalyzedParsedFile(AnalyzedParsedFile *apf) {
  if(apf == NULL) {
    printf("Error: expected a type, but got NULL\n");
    exit(1);
  }
  Operation *op = apf->get_data();
  switch(op->type) {
    case OP_TOKEN:
      return op->tk.type;
    case OP_FUNC_CALL:
      return op->funcCall.returnType;
    case OP_FUNC_DEF:
      return op->funcDef.type;
    case OP_ELEM_LIST:
      return op->elemList.elemType;
    case OP_DEREF:
      return op->deref.type;
    case OP_REF:
      return op->ref.type;
    case OP_ACCESS_FIELD:
      {
        Type t = getTypeFromAnalyzedParsedFile(op->accessField.field);
        if(t.base == TYPE_INT && op->accessField.field->get_data()->type == OP_TOKEN
          && op->accessField.field->get_data()->tk.tk->type == TK_INT) { //tuple access
          Type s = getTypeFromAnalyzedParsedFile(op->accessField.root);
          return s.subTypes[stoi(op->accessField.field->get_data()->tk.tk->text)];
        }
        else
          return t;
      }
    break;
    case OP_TYPE_DEF:
    case OP_LOOP:
    case OP_COND:
    case OP_VAR_DEF:
    case OP_MATCH:
      return Type{ .textType = "none" , .base = TYPE_NONE,  .subTypes = {}};
    default:
      printf("GetTypeFromAnalyzedParsedFile: unknown type\n");
      exit(1);
  }
}

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
    case TYPE_REF_VAR:
      {
        Type s = t.subTypes[0];
        if(s.base >= TYPE_FUNC && s.base <= TYPE_COMPOUND)
          return (t.base == TYPE_REF ? "#(" : "#mut(" )  + typeString(s) + ")";
        return (t.base == TYPE_REF ? "#" : "#mut ") + typeString(s);
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
  Type t;
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
    t = Type{ .textType = "", .base = types[type - TK_TYPE_FN_ARROW], .subTypes = subTypes }; 
  } else if(type == TK_TYPE_REF) {
    size_t child = tokens->get_neighbors_size() > CHILD(1) ? CHILD(1) : RIGHT_LINK;
    t = analyzeType(tokens->get_neighbor(child));
    t = Type{.textType = "", .base = TYPE_REF, .subTypes = {t}};
  } else if(type == TK_TYPE_INT) {
    return Type{.textType = "int", .base = TYPE_INT, .subTypes = {}};
  } else if(type == TK_TYPE_BYTE) {
    return Type{.textType = "byte", .base = TYPE_BYTE, .subTypes = {}};
  } else if(type == TK_TYPE_NONE) {
    return Type{.textType = "none", .base = TYPE_NONE, .subTypes = {}};
  } else if(type == TK_NAME) {
    return Type{.textType = tokens->get_data()->text, .base = TYPE_USER_DEFINED, .subTypes = {}};
  } else if(type == TK_ROU_BRA_OPEN) {
    t = analyzeType(tokens->get_neighbor(CHILD(1)));
  } else {
    return Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};
  }
  t.textType = typeString(t);
  return t;
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

  confirmTypeErrPos = Position(tokens->get_data()->l, tokens->get_data()->c);
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
  //this will be used on generator to know the variables passed with context
  funcDef.defsFromPrevScopes = defs.scopes.back().defsFromPrevScope;
  defs.popDefinitions(); //pop the variables of the scope

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

  if(t->base != TYPE_UNKNOWN && t->base != TYPE_REF) 
    for(int i = 0; i < (int)t->subTypes.size(); i++)
      if(t->subTypes[i].base == TYPE_FUNC) {
          printf("Error: Function as subtype at line: %d, column: %d\n", (int)confirmTypeErrPos.l, (int)confirmTypeErrPos.e);
          printf("Instead of passing a function, pass a reference to it! Try add '#'.\n");
          exit(1);
        }
  if(s->base != TYPE_UNKNOWN && s->base != TYPE_REF) 
    for(int i = 0; i < (int)s->subTypes.size(); i++)
      if(s->subTypes[i].base == TYPE_FUNC) {
          printf("Error: Function as subtype at line: %d, column: %d\n", (int)confirmTypeErrPos.l, (int)confirmTypeErrPos.e);
          printf("Instead of passing a function, pass a reference to it! Try add '#'.\n");
          exit(1);
        }

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
    else if(t->base != s->base) return false;
    else { //same base type
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

bool validLeftHandAssignment(AnalyzedParsedFile *node) {
  Operation *op = node->get_data();
  switch(op->type) {
    case OP_TOKEN:
    {
      if(op->tk.tk->type != TK_NAME) return false;
      return true;
    }
    case OP_DEREF:
    {
      Type t = getTypeFromAnalyzedParsedFile(op->deref.expr);
      if(t.base == TYPE_REF_VAR) return true;
      printf("Left Hand Assignment Error: Assign through a constant reference.\n");
      return false;
    }
    case OP_ACCESS_FIELD:
    {
      // this will change with the existence of namespaces, here we only check for tuples
      if(op->accessField.root->get_data()->type == OP_TOKEN && op->accessField.root->get_data()->tk.tk->type == TK_NAME) {
        Variable v = *defs.findDefinition(op->accessField.root->get_data()->tk.tk->text);
        if(v.mut) return true;
        printf("Left Hand Assignment Error: Assign to constant variable.\n");
        return false;
      }
      return validLeftHandAssignment(op->accessField.root);
    }
    default:
      return false;
  }
}

AnalyzedParsedFile *analyzeVar(ParsedFile *tokens) {
  OperationVarDef varDef;
  ParsedFile *tmp = tokens;
  bool assignVariable = false;
  ParsedFile *name;
  // cout << "Var: " << tokens->get_data()->text << endl;
  if(tokens->get_data()->type == TK_CONSTANT || tokens->get_data()->type == TK_VARIABLE) {
    //mutable?
    varDef.var.mut = tokens->get_data()->type == TK_VARIABLE;
    //name of the variable
    name = tmp->get_neighbor(CHILD(1));
    varDef.var.name = name->get_data()->text;
    varDef.var.pos = Position(name->get_data()->l, name->get_data()->c);
    varDef.var.id = name->get_data()->id;

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
    name = tmp->get_neighbor(CHILD(1));
    varDef.var = analyzeParseType(tokens);
    tmp = tmp->get_neighbor(RIGHT_LINK);
    tmp = tmp->get_neighbor(CHILD(1));
  } else if(tokens->get_data()->type == TK_ASSIGN) {
    name = tmp->get_neighbor(CHILD(1));
    Variable *v;
    if((v = defs.findDefinition(name->get_data()->text)) != NULL && v->mut) {
      varDef.var = *v;
      assignVariable = true;
    } else {
      if(name->get_data()->type == TK_NAME) { // a variable that we don't know the type yet
        varDef.var.mut = false;
        varDef.var.name = name->get_data()->text;
        varDef.var.pos = Position(name->get_data()->l, name->get_data()->c);
        varDef.var.id = name->get_data()->id;
        varDef.var.type = Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};
      } else { 
        // a expression that uses a variable that we know the type: @a, b.1 ...
        varDef.leftHandAssignment = analyzeParsedFile(name);
        varDef.var.type = getTypeFromAnalyzedParsedFile(varDef.leftHandAssignment);
        assignVariable = true;
      }
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
    Type valueType = getTypeFromAnalyzedParsedFile(varDef.value);

    //confront variable type with value type
    if(!confirmType(&varDef.var.type, &valueType)) {
      printf("Error: type mismatch at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
      printf("Expected: %s, Found: %s\n", varDef.var.type.textType.c_str(), valueType.textType.c_str());
      exit(1);
    }
    if(!assignVariable)
      defs.addDefinition(varDef.var); //push the variable to the scope
  }
  if(!varDef.leftHandAssignment)
    varDef.leftHandAssignment = analyzeParsedFile(name);
  if(!validLeftHandAssignment(varDef.leftHandAssignment)) {
    printf("Error: invalid left hand assignment at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
    exit(1);
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

AnalyzedParsedFile *analyzeMatch(ParsedFile *tokens) {
  OperationMatch match;
  match.expr = analyzeParsedFile(tokens->get_neighbor(CHILD(1)));
  if(match.expr->get_data()->tk.type.base != TYPE_TAG_UNION) {
    printf("Error: expected a tagged union at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
    exit(1);
  }
  ParsedFile *tmp = tokens->get_neighbor(RIGHT_LINK);
  for(int branch = CHILD(1); branch < (int)tmp->get_neighbors_size(); branch++) {
    ParsedFile *node = tmp->get_neighbor(branch);
    Variable v = analyzeParseType(node->get_neighbor(CHILD(1)));
    defs.scopes.push_back(Scope(SCOPE_MATCH_BRANCH));
    defs.addDefinition(v);
    match.castedVars.push_back(v);
    match.branches.push_back({});
    for(int i = CHILD(2); i < (int)node->get_neighbors_size(); i++) {
      if(node->get_neighbor(i) == NULL) continue;
      match.branches.back().push_back(analyzeParsedFile(node->get_neighbor(i)));
    }
    defs.popDefinitions();
  }
  return new AnalyzedParsedFile(new Operation(match, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

AnalyzedParsedFile *analyzeListOfElements(ParsedFile *tokens) {
  OperationElemList elemList;
  elemList.type = tokens->get_data()->type == TK_SQR_BRA_OPEN ? OperationElemList::ARRAY : OperationElemList::TUPLE;
  elemList.elemType = elemList.type == OperationElemList::ARRAY ? Type{"", TYPE_REF, {}} : Type{"", TYPE_COMPOUND, {}};
  TokenType lastElement = elemList.type == OperationElemList::ARRAY ? TK_SQR_BRA_CLOSE : TK_CUR_BRA_CLOSE;

  ParsedFile *tmp = tokens;
  while(tmp->get_data()->type != lastElement) {
    AnalyzedParsedFile *node = analyzeParsedFile(tmp->get_neighbor(CHILD(1)));
    Type type = getTypeFromAnalyzedParsedFile(node);
    elemList.values.push_back(node);

    if(elemList.elemType.base == TYPE_COMPOUND)
      elemList.elemType.subTypes.push_back(type);
    else if(elemList.elemType.base == TYPE_REF) {
      if(elemList.elemType.subTypes.empty()) {
        elemList.elemType.subTypes.push_back(type);
      } else if(!confirmType(&elemList.elemType.subTypes[0], &type)) {
        printf("Error: type mismatch at line: %d, column: %d\n", (int)tmp->get_data()->l, (int)tmp->get_data()->c);
        printf("Expected: %s, Found: %s\n", elemList.elemType.subTypes[0].textType.c_str(), type.textType.c_str());
        exit(1);
      }
    }
    tmp = tmp->get_neighbor(RIGHT_LINK);
  }
  elemList.elemType.textType = typeString(elemList.elemType);

  return new AnalyzedParsedFile(new Operation(elemList, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

AnalyzedParsedFile *analyzeDeref(ParsedFile *tokens) {
  OperationDeref deref;
  deref.expr = analyzeParsedFile(tokens->get_neighbor(CHILD(1)));
  Type innerType = getTypeFromAnalyzedParsedFile(deref.expr);
  if(innerType.base != TYPE_REF && innerType.base != TYPE_REF_VAR) {
    printf("AnalyzeDeref Error: expected a reference type at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
    printf("Found: %s\n", innerType.textType.c_str());
    exit(1);
  }
  deref.type = innerType.subTypes[0];
  deref.type.textType = typeString(deref.type);
  deref.offset = tokens->get_neighbors_size() > CHILD(2) ? stoi(tokens->get_neighbor(CHILD(2))->get_data()->text) : 0;

  return new AnalyzedParsedFile(new Operation(deref, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

AnalyzedParsedFile *analyzeRef(ParsedFile *tokens) {
  OperationRef ref;
  uint8_t typeOfRef = 0; //0 for constant reference
  if(tokens->get_neighbors_size() == CHILD(1) &&
     tokens->get_neighbor(RIGHT_LINK)->get_data()->type == TK_VARIABLE)
  {
    tokens = tokens->get_neighbor(RIGHT_LINK);
    typeOfRef++; //1 for mutable reference
  }
  ref.expr = analyzeParsedFile(tokens->get_neighbor(CHILD(1)));
  if(ref.expr->get_data()->type == OP_TOKEN && ref.expr->get_data()->tk.tk->type == TK_NAME) {
    Variable v = *defs.findDefinition(ref.expr->get_data()->tk.tk->text);
    if(!v.mut && typeOfRef == 1) {
      printf("Reference Error: Cannot create an variable reference to a constant at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
      exit(1);
    }
    ref.type = Type{.textType = "", .base = (typeOfRef == 1 ? TYPE_REF_VAR : TYPE_REF), .subTypes = {v.type}};
    ref.type.textType = typeString(ref.type);
  }
  return new AnalyzedParsedFile(new Operation(ref, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

AnalyzedParsedFile *analyzeAccessField(ParsedFile *tokens) {
  OperationAccessField acField;
  acField.root = analyzeParsedFile(tokens->get_neighbor(CHILD(1)));
  acField.field = analyzeParsedFile(tokens->get_neighbor(CHILD(2)));
  Type rootType = getTypeFromAnalyzedParsedFile(acField.root);
  if(rootType.base == TYPE_COMPOUND)
  {
    if(acField.field->get_data()->type != OP_TOKEN || acField.field->get_data()->tk.type.base != TYPE_INT) {
      printf("Tuple field access error: expected an integer at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
      exit(1);
    }
    int number = stoi(acField.field->get_data()->tk.tk->text);
    if(number >= (int)rootType.subTypes.size() || number < 0) {
      printf("Error: index out of bounds at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
      exit(1);
    }
    acField.type = rootType.subTypes[number];
  } else {
    acField.type = getTypeFromAnalyzedParsedFile(acField.field);
  }

  return new AnalyzedParsedFile(new Operation(acField, Position(tokens->get_data()->l, tokens->get_data()->c)));
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
    case TK_DOT:
    {
        AnalyzedParsedFile *child = analyzeParsedFile(tokens->get_neighbor(CHILD(1)));
        AnalyzedParsedFile *child2 = analyzeParsedFile(tokens->get_neighbor(CHILD(2)));
        Type childType = getTypeFromAnalyzedParsedFile(child);
        if(childType.base == TYPE_COMPOUND && child2->get_data()->type == OP_TOKEN && child2->get_data()->tk.type.base == TYPE_INT) {
          int number = stoi(child2->get_data()->tk.tk->text);
          if(number >= (int)childType.subTypes.size() || number < 0) {
            printf("Error: index out of bounds at line: %d, column: %d\n", (int)tokens->get_data()->l, (int)tokens->get_data()->c);
            exit(1);
          }
          t = childType.subTypes[number];
        }
        AnalyzedParsedFile *node = new AnalyzedParsedFile(
          new Operation(OperationToken{.tk = tokens->get_data(), .type = t}, Position(tokens->get_data()->l, tokens->get_data()->c))
        );
        AnalyzedParsedFile::linkFatherAndChild(node, child);
        AnalyzedParsedFile::linkFatherAndChild(node, child2);
        return node;
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
        if(tokens->get_data()->type == TK_LOG_NOT || tokens->get_data()->type == TK_BIT_NOT) {
          t = left->get_data()->tk.type;
          OperationToken tk = OperationToken{.tk = tokens->get_data(), .type = t};
          AnalyzedParsedFile *node = new AnalyzedParsedFile(
            new Operation(tk, Position(tokens->get_data()->l, tokens->get_data()->c))
          );
          AnalyzedParsedFile::linkFatherAndChild(node, left);
          return node;
        }
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
    case TK_BLOCK_MATCH:
      return analyzeMatch(tokens);
    case TK_SQR_BRA_OPEN:
    case TK_CUR_BRA_OPEN:
      return analyzeListOfElements(tokens);
    case TK_NAME:
      if(tokens->get_neighbor(RIGHT_LINK) && tokens->get_neighbor(RIGHT_LINK)->get_data()->type == TK_ROU_BRA_OPEN)
        return analyzeFuncCall(tokens);
      return analyzeToken(tokens);
    case TK_TYPE_DEREF:
      if(tokens->get_neighbor(RIGHT_LINK) && tokens->get_neighbor(RIGHT_LINK)->get_data()->type == TK_ROU_BRA_OPEN)
        return analyzeFuncCall(tokens);
      return analyzeDeref(tokens);
    case TK_TYPE_REF:
      return analyzeRef(tokens);
    case TK_DOT:
      return analyzeAccessField(tokens);
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
          cout << tab << "Variable def of type: " << op->varDef.var.type.textType << endl;
        else
          cout << tab << "Constant def of type: " << op->varDef.var.type.textType << endl;
        cout << tab+"  " << "Assigning to: \n";
        printAnalyzerParsedFile(op->varDef.leftHandAssignment, tab+"    "); 
        cout << tab+"  " << "Value: \n";
        printAnalyzerParsedFile(op->varDef.value, tab+"    ");
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
    case OP_MATCH:
    {
        OperationMatch match = op->match;
        cout << tab << "Match: " << endl;
        for(int i = 0; i < (int)match.castedVars.size(); i++) {
          cout << tab+"  " << "-> Casted var: " << match.castedVars[i].name << " Type: " << match.castedVars[i].type.textType << endl;
          cout << tab+"  " << "Branch: " << endl;
          for(auto b : match.branches[i]) {
            printAnalyzerParsedFile(b, tab+"    ");
          }
        }
    }
    break;
    case OP_ELEM_LIST:
    {
      OperationElemList elemList = op->elemList;
      cout << tab << "List of elements: " << (elemList.type == OperationElemList::ARRAY ? "Array" : "Tuple") << endl;
      for(auto e : elemList.values) {
        printAnalyzerParsedFile(e, tab+"  ");
      }
    }
    break;
    case OP_DEREF:
    {
      OperationDeref deref = op->deref;
      cout << tab << "Deref with offset: " << deref.offset << endl;
      cout << tab+"  " << "Expr of type " << deref.type.textType << " :\n";

      printAnalyzerParsedFile(deref.expr, tab+"    ");
    }
    break;
    case OP_REF:
    {
      OperationRef ref = op->ref;
      if(ref.type.base == TYPE_REF)
        cout << tab << "Ref: " << endl;
      else
        cout << tab << "Mutable ref: " << endl;
      cout << tab+"  " << "Expr of type " << ref.type.textType << " :\n";
      printAnalyzerParsedFile(ref.expr, tab+"    ");
    }
    break;
    case OP_ACCESS_FIELD:
    {
      OperationAccessField acField = op->accessField;
      cout << tab << "Access field. Return type: " << acField.type.textType << endl;
      cout << tab+"  " << "Root: " << endl;
      printAnalyzerParsedFile(acField.root, tab+"    ");
      cout << tab+"  " << "Field: " << endl;
      printAnalyzerParsedFile(acField.field, tab+"    ");
    }
    break;
    default:
      printf("not implemented yet\n");
      exit(1);
  }
}

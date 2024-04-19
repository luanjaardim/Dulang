#include "analyzer.h"
#include "utils.h"

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
      return "#" + typeString(t.subTypes[0]);
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
  while(tmp->get_data()->type != TK_END_BAR) {
    if(tmp->get_neighbors_size() >= CHILD(1)) {
      funcDef.args.push_back(analyzeParseType(tmp->get_neighbor(CHILD(1)))); //get the arguments
      t.subTypes.push_back(funcDef.args.back().type); //get to make the type of the function
    }
    tmp = tmp->get_neighbor(RIGHT_LINK);
  }
  t.subTypes.push_back(Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}}); //return type
  //get the function type as text
  t.textType = typeString(t);
  funcDef.type = t;

  //goes to the operations
  for(int i = CHILD(1); i < (int)tmp->get_neighbors_size(); i++) {
    if(tmp->get_neighbor(i)->get_data() == NULL) continue;
    funcDef.ops.push_back(analyzeParsedFile(tmp->get_neighbor(i)));
  }

  return new AnalyzedParsedFile(new Operation(funcDef, Position(tokens->get_data()->l, tokens->get_data()->c)));
}

AnalyzedParsedFile *analyzeVar(ParsedFile *tokens) {
  OperationVarDef varDef;
  ParsedFile *tmp = tokens;
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
    varDef.var.mut = false;
    Token *name = tmp->get_neighbor(CHILD(1))->get_data();
    varDef.var.name = name->text;
    varDef.var.pos = Position(name->l, name->c);
    varDef.var.id = name->id;
    varDef.var.type = Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};
    tmp = tmp->get_neighbor(CHILD(2));
  }

  //goes to the value of the variable
  varDef.value = analyzeParsedFile(tmp);
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
    cond.expr = analyzeToken(child);
    //goes to the operations
    tmp = tmp->get_neighbor(RIGHT_LINK);
    condType = condType == 0 ? OperationCond::IF : OperationCond::ELSE_IF;
  }
  //get body
  for(int i = CHILD(1); i < (int)tmp->get_neighbors_size(); i++) {
    if(tmp->get_neighbor(i) == NULL) continue;
    cond.ops.push_back(analyzeParsedFile(tmp->get_neighbor(i)));
  }
  cond.type = condType;
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
  for(int i = CHILD(1); i < (int)tmp->get_neighbors_size(); i++) {
    if(tmp->get_neighbor(i)->get_data() == NULL) continue;
    loop.ops.push_back(analyzeParsedFile(tmp->get_neighbor(i)));
  }
  return new AnalyzedParsedFile(new Operation(loop, Position(tokens->get_data()->l, tokens->get_data()->c)));
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
      { t = Type{.textType = tokens->get_data()->text, .base = TYPE_USER_DEFINED, .subTypes = {}}; }
    break;
    default:
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
        OperationToken expr = op->loop.expr->get_data()->tk;
        cout << tab << "Loop, Expression: " << expr.tk->text << " Type: " << expr.type.textType << endl;
        printAnalyzerParsedFile(op->cond.expr, tab+"  ");
        cout << tab+"  " << "Operations: " << endl;
        for(auto o : op->loop.ops) {
          printAnalyzerParsedFile(o, tab+"    ");
        }
        break;

      }
  }
}

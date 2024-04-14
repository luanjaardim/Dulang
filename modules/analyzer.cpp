#include "analyzer.h"
#include "utils.h"

Type analyzeType(ParsedFile *tokens) {
  if(tokens == NULL)
    return Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};

  TokenType type = tokens->get_data()->type;
  if(type == TK_TYPE_FN_ARROW || type == TK_TYPE_TAG_UNION) {
    Type firstChild = analyzeType(tokens->get_neighbor(CHILD(1)));
    Type secondChild = analyzeType(tokens->get_neighbor(CHILD(2)));
    return Type{
      .textType = firstChild.textType + (type == TK_TYPE_FN_ARROW ? " -> " : " ^ ") + secondChild.textType,
      .base = TYPE_COMPOUND,
      .subTypes = {firstChild, secondChild}
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
  } else {
    return Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};
  }
}

Type analyzeExprType(ParsedFile *tokens) {
  (void)tokens;
  return Type{.textType = "unknown", .base = TYPE_UNKNOWN, .subTypes = {}};
}

// must be a name followed by a type as a child, and it will return a Variable
Variable analyzeNameAndType(ParsedFile *tokens) {
  return Variable{
    .id = tokens->get_data()->id,
    .name = tokens->get_data()->text,
    .mut = false,
    .pos = Position(tokens->get_data()->l, tokens->get_data()->c),
    .type = analyzeType(tokens->get_neighbor(CHILD(1)))
  };
}

AnalyzedParsedFile *analyzeFunc(ParsedFile *tokens) {
  ParsedFile *tmp = tokens;
  OperationFuncDef funcDef;
  funcDef.returnType = analyzeType(tokens->get_neighbor(CHILD(1)));
  //goes to the name of the function
  tmp = tmp->get_neighbor(RIGHT_LINK);
  funcDef.name = tmp->get_data()->text;

  //goes to the arguments, and discarts ':' if it has no arguments
  tmp = tmp->get_neighbor(RIGHT_LINK)->get_neighbors_size() >= 3 ?
        tmp->get_neighbor(RIGHT_LINK) :
        tmp->get_neighbor(RIGHT_LINK)->get_neighbor(RIGHT_LINK);

  //goes to the arguments
  while(tmp->get_data()->type != TK_END_BAR) {
    //get the arguments
    funcDef.args.push_back(analyzeNameAndType(tmp->get_neighbor(CHILD(1))));
    //analyze Var and Type to have a OperationToken with the Token to the arg name and the Type
    tmp = tmp->get_neighbor(RIGHT_LINK);
  }
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
    //type of the variable
    varDef.var.type = analyzeType(tmp->get_neighbor(CHILD(1)));

    //name of the variable
    tmp = tmp->get_neighbor(RIGHT_LINK);
    varDef.var.name = tmp->get_data()->text;
    varDef.var.id = tmp->get_data()->id;
  } else if(tokens->get_data()->type == TK_NAME)
    varDef.var = analyzeNameAndType(tokens);

  //goes to the value of the variable
  tmp = tmp->get_neighbor(RIGHT_LINK)->get_neighbor(CHILD(1));
  AnalyzedParsedFile *node = analyzeParsedFile(tmp);// TODO: delete node
  if(node->get_data()->type != OP_TOKEN) {
    printf("Error: variable must have a value\n");
    exit(1);
  }
  varDef.value = node->get_data()->tk;
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
    cond.expr.tk = NULL;
    cond.expr.type = Type{.textType = "none", .base = TYPE_NONE, .subTypes = {}};
    condType = OperationCond::ELSE;
  }
  if(tmp->get_data()->type == TK_BLOCK_IF ) {
    ParsedFile *child = tmp->get_neighbor(CHILD(1));
    // TODO: remove this by not allowing expression to be empty
    if(child == NULL) {
      printf("Error: if without condition at line: %d, column: %d\n", (int)tmp->get_data()->l, (int)tmp->get_data()->c);
      exit(1);
    }
    cond.expr = OperationToken{.tk = child->get_data(), .type = analyzeExprType(child)};
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
    loop.expr = OperationToken{.tk = tmp->get_data(), .type = analyzeExprType(tmp)};
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
  OperationToken tk = OperationToken{.tk = tokens->get_data(), .type = analyzeType(tokens)};
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
        return analyzeVar(tokens);
    case TK_NAME:
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
      cout << tab << "Function: " << op->funcDef.name << " Type: " << op->funcDef.returnType.textType << endl;
      for(auto arg : op->funcDef.args) {
        cout << tab+"  " << "Arg: " << arg.name << " Type: " << arg.type.textType << endl;
      }
      cout << tab << "Operations: " << endl;
      for(auto o : op->funcDef.ops) {
        printAnalyzerParsedFile(o, tab+"  ");
      }
      break;
    case OP_VAR_DEF:
      cout << tab << "Variable def: " << op->varDef.var.name << " Type: " << op->varDef.var.type.textType << endl;
      cout << tab+"  " << "Value: " << op->varDef.value.tk->text << " Type: " << op->varDef.value.type.textType << endl;
      break;
    case OP_COND:
      {
        vector<string> condTypes = {"NONE", "IF", "ELSE", "ELSE_IF"};
        cout << tab << "Cond: " << condTypes[op->cond.type] << endl;
        if(op->cond.expr.tk)
          cout << tab << " Expr: " << op->cond.expr.tk->text << " Type: " << op->cond.expr.type.textType << endl;
        cout << tab << "Operations: " << endl;
        for(auto o : op->cond.ops) {
          printAnalyzerParsedFile(o, tab+"  ");
        }
      }
      break;
    case OP_LOOP:
      cout << tab << "Loop: " << op->loop.expr.tk->text << " Type: " << op->loop.expr.type.textType << endl;
      for(auto o : op->loop.ops) {
        printAnalyzerParsedFile(o, tab+"  ");
      }
      break;
  }
}

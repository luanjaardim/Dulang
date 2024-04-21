#include "generator.h"
#include "utils.h"

string typeAsCType(Type t) {
  switch(t.base) {
    case TYPE_INT:
      return "int";
    case TYPE_BYTE:
      return "char";
    case TYPE_REF:
      return typeAsCType(t.subTypes[0]) + "*";
    case TYPE_NONE:
      return "void";
    default:
    break;
  }
  return "";
}

string convertToCVariable(Variable v) {
  switch(v.type.base) {
    case TYPE_INT:
    case TYPE_BYTE:
    case TYPE_REF:
    case TYPE_NONE:
      return typeAsCType(v.type) + " " + v.name;
    case TYPE_FUNC:
    {
      Type t = v.type;
      string returnType = typeAsCType(t.subTypes[t.subTypes.size() - 1]);
      string name = "(*" + v.name + ")(";
      string type = returnType + name;
      for(int i = 0; i < (int)t.subTypes.size() - 1; i ++) {
          type += typeAsCType(t.subTypes[i]);
          if(i != (int)t.subTypes.size() - 1) type += ",";
      }
      return type;
    }
    case TYPE_COMPOUND:
    case TYPE_TAG_UNION:
    default:
      printf("not implemented yet\n");
      exit(1);
      break;
  }
}

string Generator::convertASTtoC(AnalyzedParsedFile *ast) {
  if(ast == NULL) return "";
  printAnalyzerParsedFile(ast, "");
  Operation *op = ast->get_data();
  string text;
  switch(op->type) {
    case OP_FUNC_DEF:
    {
      Variable f = getLastDefinition();
      OperationFuncDef fnDef = ast->get_data()->funcDef;
      text = typeAsCType(f.type.subTypes[f.type.subTypes.size() - 1]);
      text += " " + f.name + "(";
      for(auto v : fnDef.args) {
        text += convertToCVariable(v);
      }
      text += ") {\n";

      for(auto op : fnDef.ops)
        text += this->convertASTtoC(op);

      text += "\n}";
    }
    break;
    case OP_VAR_DEF:
      if(op->varDef.var.type.base == TYPE_FUNC) {
        this->scopes.push_back(Scope(SCOPE_FUNC));
        this->addDefinition(op->varDef.var);    //start of function scope
        text = this->convertASTtoC(op->varDef.value);
        this->popDefinition();                  //end of function scope
      } else {
        this->addDefinition(op->varDef.var);
        text = convertToCVariable(op->varDef.var) + " = " + this->convertASTtoC(op->varDef.value) + ";\n";
        text += this->convertASTtoC(ast->get_neighbor(RIGHT_LINK));
      }
    break;
    case OP_FUNC_CALL:
    {
      OperationFuncCall fnCall = op->funcCall;
      text = fnCall.funcName + "(";
      for(int i = 0; i < (int)fnCall.params.size(); i++) {
        text += this->convertASTtoC(fnCall.params[i]);
        if(i != (int)fnCall.params.size() - 1) text += ",";
      }
      text += ")";
    }
    break;
    case OP_TOKEN:
    {
      OperationToken tk = op->tk;
      switch(tk.tk->type) {
        case TK_BLOCK_BACK: text = "return " + this->convertASTtoC(ast->get_neighbor(CHILD(1))) + ";\n"; break;
        case TK_BLOCK_SKIP: text = "continue;\n"; break; // TODO: continue for outter loops
        case TK_BLOCK_STOP: text = "break;\n"; break; // TODO: break for outter loops

        //operations with two operands
        case TK_NUM_ADD:
        case TK_NUM_SUB:
        case TK_NUM_MUL:
        case TK_NUM_DIV:
        case TK_NUM_MOD:
        {
          text = this->convertASTtoC(ast->get_neighbor(CHILD(1))) + tk.tk->text + this->convertASTtoC(ast->get_neighbor(CHILD(2)));
        } break;
        case TK_TYPE_REF:
        case TK_TYPE_DEREF:
        {
          text = (tk.tk->type == TK_TYPE_DEREF ? "*" : "&") + this->convertASTtoC(ast->get_neighbor(CHILD(1)));
        } break;
        default:
          text = tk.tk->text;
      }
    }
    break;
    default:
      printf("not implemented yet:\n");
      printAnalyzerParsedFile(ast, "");
      exit(1);
  }
  return text;
}

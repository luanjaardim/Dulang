#include "generator.h"
#include "utils.h"

DefinitionsHandler defsGen;

string getVariableName(Variable v) {
  return v.name + "_" + to_string(v.id);
}

string typeAsCType(Type t) {
  switch(t.base) {
    case TYPE_INT:
      return "int";
    case TYPE_BYTE:
      return "char";
    case TYPE_NONE:
      return "void";
    case TYPE_REF:
    {
      string innerType = typeAsCType(t.subTypes[0]);
      size_t pos;
      if((pos = innerType.find('$')) != string::npos) {
          return innerType.insert(pos, "*");
      }
      return innerType + "*";
    }
    case TYPE_FUNC:
    {
      // $ will be used as place to put the name of the variable after the type is finished
      string type = typeAsCType(t.subTypes[t.subTypes.size() - 1]) + " ($)(";
      for(int i = 0; i < (int)t.subTypes.size() - 1; i ++) {
          type += typeAsCType(t.subTypes[i]);
          if(i != (int)t.subTypes.size() - 2) type += ",";
      }
      return type + ")";
    }
    case TYPE_UNKNOWN:
      return "unknown";
    default:
    break;
  }
  return "";
}

string convertToCVariable(Variable v) {
  string nameAndId = getVariableName(v);
  switch(v.type.base) {
    case TYPE_INT:
    case TYPE_BYTE:
    case TYPE_REF:
    case TYPE_NONE:
      {
      string text = typeAsCType(v.type);
        size_t pos;
      if((pos = text.find('$')) != string::npos)
        text.replace(pos, 1, nameAndId);
      else
        text += " " + nameAndId;
      return text;
      }
    case TYPE_COMPOUND:
    case TYPE_TAG_UNION:
    case TYPE_FUNC: //probably wont be needed
    default:
      printf("this type is not implemented yet: %s\n", typeAsCType(v.type).c_str());
      exit(1);
      break;
  }
}

string Generator::createFunc(Variable f, vector<Variable> args) {
  string text = typeAsCType(f.type.subTypes[f.type.subTypes.size() - 1]);
  text += " " + getVariableName(f) + "(";
  for(auto v : args) {
    text += convertToCVariable(v);
    text += v.id != args[args.size() - 1].id ? "," : "";
    defsGen.addDefinition(v);
  }
  text += ") {\n";
  return text;
}

string Generator::convertASTtoC(AnalyzedParsedFile *ast) {
  if(ast == NULL) return "";
  // printAnalyzerParsedFile(ast, "");
  Operation *op = ast->get_data();
  string text = "";
  switch(op->type) {
    case OP_FUNC_DEF:
    {
      Variable f = *defsGen.getLastDefinition();
      OperationFuncDef fnDef = ast->get_data()->funcDef;
      text = createFunc(f, fnDef.args);

      for(auto op : fnDef.ops)
        text += "  " + this->convertASTtoC(op);

      text += "}\n";
    }
    break;
    case OP_VAR_DEF:
      if(op->varDef.var.type.base == TYPE_FUNC && 
        (op->varDef.value->get_data()->type == OP_FUNC_DEF ||
        op->varDef.value->get_data()->type == OP_FUNC_CALL)
      ){
        defsGen.addDefinition(op->varDef.var);
        defsGen.scopes.push_back(Scope(SCOPE_FUNC));      //start of function scope
        text = this->convertASTtoC(op->varDef.value);
        defsGen.popDefinitions();                         //end of function scope
      } else {
        if(op->varDef.var.type.base == TYPE_FUNC) {
          printf("When passing a function to another constant, use a pointer to a function! Try add an '#'.\n");
          printf("Error: type mismatch at line: %d, column: %d\n", (int)op->varDef.value->get_data()->pos.l, (int)op->varDef.value->get_data()->pos.e);
          exit(1);
        }
        Variable *v;
        if((v = defsGen.findDefinition(op->varDef.var.name)) != NULL && v->mut && op->varDef.var.mut)
          text = getVariableName(*v) + " = " + this->convertASTtoC(op->varDef.value) + ";\n";
        else {
          defsGen.addDefinition(op->varDef.var);
          text = convertToCVariable(op->varDef.var) + " = " + this->convertASTtoC(op->varDef.value) + ";\n";
        }

        text += this->convertASTtoC(ast->get_neighbor(RIGHT_LINK));
      }
    break;
    case OP_FUNC_CALL:
    {
      OperationFuncCall fnCall = op->funcCall;
      string caller = this->convertASTtoC(op->funcCall.func);
      //check if the call is by a function pointer
      bool isFuncPointerCall = caller[0] == '*' || caller[caller.size() - 1] == ']';

      vector<Variable> args;
      if(fnCall.returnType.base == TYPE_FUNC) {
          Variable v = *defsGen.getLastDefinition();
          for(int i = 0; i < (int)v.type.subTypes.size()-1; i++)
            args.push_back(Variable{
              .id = v.id,
              .name = "dummy" + to_string(i),
              .pos = v.pos,
              .type = v.type.subTypes[i],
            });
          text = createFunc(v, args) + "\n\treturn ";
      }
      text += (isFuncPointerCall ? "(" + caller + ")" : caller) + "(";
      for(int i = 0; i < (int)fnCall.params.size(); i++) {
        text += this->convertASTtoC(fnCall.params[i]);
        if(fnCall.returnType.base == TYPE_FUNC || i != (int)fnCall.params.size() - 1) text += ",";
      }
      if(fnCall.returnType.base == TYPE_FUNC) {
        for(int i = 0; i < (int)args.size(); i++)
          text += getVariableName(args[i]) + (i != (int)args.size() - 1 ? "," : ");\n");
        text += "}\n";
      }
      else
        text += ")";
    }
    break;
    case OP_COND:
    {
      OperationCond cond = op->cond;
      string conditions[3] = {"if", "else", "else if"};
      text = conditions[cond.type-1];
      if(cond.type != OperationCond::ELSE)
        text += "(" + this->convertASTtoC(cond.expr) + ") {\n";
      else 
        text += "{\n";

      defsGen.scopes.push_back(Scope(SCOPE_COND));
      for(auto op : cond.ops)
        text += "  " + this->convertASTtoC(op);
      text += "\n}\n";
    }
    break;
    case OP_LOOP:
    {
      OperationLoop loop = op->loop;
      text = "while(" + this->convertASTtoC(loop.expr) + ") {\n";
      defsGen.scopes.push_back(Scope(SCOPE_LOOP));
      for(auto op : loop.ops)
        text += "  " + this->convertASTtoC(op);
      text += "}\n";
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
          text = this->convertASTtoC(ast->get_neighbor(CHILD(1))) + " " +tk.tk->text + " " + this->convertASTtoC(ast->get_neighbor(CHILD(2)));
        } break;
        case TK_TYPE_REF:
        case TK_TYPE_DEREF:
        {
          AnalyzedParsedFile *child1 = ast->get_neighbor(CHILD(1));
          string child1Text = this->convertASTtoC(child1);
          size_t numberOfChildren = ast->get_neighbors_size();
          if(child1->get_data()->type == OP_TOKEN &&  //check if the previous dereference is different from the current one
             child1->get_data()->tk.tk->type == TK_TYPE_DEREF &&
             numberOfChildren != child1->get_neighbors_size()) child1Text = "(" + child1Text + ")";

          if(numberOfChildren == (CHILD(1)) + 1) //has only one child
            text = (tk.tk->type == TK_TYPE_DEREF ? "*" : "&") + child1Text;
          else {
            if(tk.tk->type != TK_TYPE_DEREF) {
              printf("Fatal error: expected a variable to be dereferenced, but got %s\n", child1->get_data()->varDef.var.name.c_str());
            } else {
              text = child1Text + "[" + this->convertASTtoC(ast->get_neighbor(CHILD(2))) + "]";
            }

          }
        } break;
        case TK_NAME:
        {
          //wont exist any null return from findDefinition, all of them were solved at analyzer, i think.
          Variable v = *defsGen.findDefinition(tk.tk->text);
          text = getVariableName(v);
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

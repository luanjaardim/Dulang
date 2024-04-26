#include "generator.h"
#include "utils.h"

DefinitionsHandler defsGen;

string getVariableName(Variable v) {
  if(v.name != "main")
    return v.name + "_" + to_string(v.id);
  return v.name;
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

string Generator::convertToCVariable(Variable v) {
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
    case TYPE_TAG_UNION:
      return "struct taggedUnion" + to_string(getTaggedUnionOrTuppleId(v.type)) + " " + nameAndId;
    case TYPE_COMPOUND:
      return "struct tupple" + to_string(getTaggedUnionOrTuppleId(v.type)) + " " + nameAndId;
    case TYPE_FUNC: //probably wont be needed
    default:
      printf("Error at convertToCVariable: this type is not implemented yet: %s\n", typeAsCType(v.type).c_str());
      exit(1);
      break;
  }
}

size_t Generator::fromTaggedUnionIdGetTypeId(size_t tagUnionId, Type t) {
  if(t.base == TYPE_UNKNOWN) {
    printf("Error: expected a type, but got unknown\n");
    exit(1);
  }
  for(size_t i = 0; i < this->definedTypes[tagUnionId].subTypes.size(); i++) {
    if(confirmType(&t, &this->definedTypes[tagUnionId].subTypes[i])) return i;
  }
  printf("Error: could not find the type %s in the tagged union of type %s\n", t.textType.c_str(), this->definedTypes[tagUnionId].textType.c_str());
  exit(1);
}

size_t Generator::getTaggedUnionOrTuppleId(Type t) {
  if(t.base != TYPE_TAG_UNION && t.base != TYPE_COMPOUND) {
    printf("Error: expected a tagged union, but got %s\n", typeAsCType(t).c_str());
    exit(1);
  }
  size_t index = 0; //the index of the defined type, so the name of the type is taggedUnion + index
  for(auto s : this->definedTypes) {
    if(confirmType(&t, &s)) return index;
    index++;
  }
  string text = (t.base == TYPE_TAG_UNION ? "struct taggedUnion" : "struct tupple") + to_string(index) + " {\n";
  if(t.base == TYPE_TAG_UNION)
    text += "  enum {\n";
  string enum_elements = "";
  string fields = "";
  for(int i = 0; i < (int)t.subTypes.size(); i++) {
    enum_elements += "    TYPE_" + to_string(i) + ",\n";
    string type = typeAsCType(t.subTypes[i]);
    if(type.find('$') != string::npos) {
      type.replace(type.find('$'), 1, "FIELD_" + to_string(i));
    } else {
      type += " FIELD_" + to_string(i);
    }
    fields += "    " + type + ";\n";
  }

  if(t.base == TYPE_TAG_UNION) {
    text += enum_elements + "  } type;\n";
    text += "  union {\n";
  }
  text += fields + (t.base == TYPE_TAG_UNION ? "  };\n" : "");
  text += "};\n";
  this->prevDefinitions += text;

  this->definedTypes.push_back(t);
  return index;
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
      string funcDef = createFunc(f, fnDef.args);

      for(auto op : fnDef.ops)
        funcDef += "  " + this->convertASTtoC(op);

      funcDef += "}\n";
      this->prevDefinitions += funcDef;
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
        if((v = defsGen.findDefinition(op->varDef.var.name)) != NULL && v->mut && op->varDef.var.mut) {
          if(op->varDef.var.type.base == TYPE_TAG_UNION) {
            Type t = getTypeFromAnalyzedParsedFile(op->varDef.value);
            size_t taggedUnionId = getTaggedUnionOrTuppleId(op->varDef.var.type);
            size_t typeId = fromTaggedUnionIdGetTypeId(taggedUnionId, t);
            string id = to_string(typeId);
            text = getVariableName(*v) +
                " = (struct taggedUnion" + id + ") {.type = TYPE_" + id + ", .FIELD_" + id + " = " +
                this->convertASTtoC(op->varDef.value) + "};\n";
          } else
            text = getVariableName(*v) + " = " + this->convertASTtoC(op->varDef.value) + ";\n";
        }
        else {
          defsGen.addDefinition(op->varDef.var);
          text = convertToCVariable(op->varDef.var) + " = ";
          string value = this->convertASTtoC(op->varDef.value);
          if(op->varDef.var.type.base == TYPE_TAG_UNION) {
            Type t = getTypeFromAnalyzedParsedFile(op->varDef.value);
            size_t taggedUnionId = getTaggedUnionOrTuppleId(op->varDef.var.type);
            size_t typeId = fromTaggedUnionIdGetTypeId(taggedUnionId, t);
            text += "{.type = TYPE_" + to_string(typeId) + ", .FIELD_" + to_string(typeId) + " = " + value + "};\n";
          } else
             text += value + ";\n";
        }

        text += this->convertASTtoC(ast->get_neighbor(RIGHT_LINK));
      }
    break;
    case OP_FUNC_CALL:
    {
      OperationFuncCall fnCall = op->funcCall;
      string caller = this->convertASTtoC(op->funcCall.func);
      string localText = "";
      //check if the call is by a function pointer
      bool isFuncPointerCall = fnCall.func->get_data()->type == OP_TOKEN && fnCall.func->get_data()->tk.tk->type == TK_TYPE_DEREF;

      vector<Variable> args;
      //the function call will create another function that uses the original one, but with the constants
      //parameters that were passed to the function
      if(fnCall.returnType.base == TYPE_FUNC) { 
          Variable v = *defsGen.getLastDefinition();
          for(int i = 0; i < (int)v.type.subTypes.size()-1; i++)
            args.push_back(Variable{
              .id = v.id,
              .name = "dummy" + to_string(i),
              .pos = v.pos,
              .type = v.type.subTypes[i],
            });
          localText = createFunc(v, args) + "  return ";
      }
      // partial function call with a function pointer
      // we will create a function as normal, but a global function pointer will be created
      // to store the function pointer, being used in the function call
      if(isFuncPointerCall && fnCall.returnType.base == TYPE_FUNC) {
        AnalyzedParsedFile *tmp = fnCall.func->get_neighbor(CHILD(1));
        while(tmp->get_data()->type == OP_TOKEN && tmp->get_data()->tk.tk->type == TK_TYPE_DEREF)
          tmp = tmp->get_neighbor(CHILD(1));
        if(tmp->get_data()->type == OP_TOKEN && tmp->get_data()->tk.tk->type == TK_NAME) {
          Variable funcPntGlobal = *defsGen.findDefinition(tmp->get_data()->tk.tk->text);
          string nameWithId = getVariableName(funcPntGlobal);
          funcPntGlobal.name = "global_" + funcPntGlobal.name;
          string globalNameWithId = getVariableName(funcPntGlobal);
          caller.replace(caller.find(nameWithId), nameWithId.size(), globalNameWithId);
          //creating the global function pointer
          this->prevDefinitions += convertToCVariable(funcPntGlobal) + " = (void *)0;\n";
          text = "if(" + globalNameWithId + " == (void *)0) " + globalNameWithId + " = " + nameWithId + ";\n";
        } else {
          printf("Error: expected a function name, but got %s\n", tmp->get_data()->tk.tk->text.c_str());
          exit(1);
        }
      }
      localText += (isFuncPointerCall ? "(" + caller + ")" : caller) + "(";
      for(int i = 0; i < (int)fnCall.params.size(); i++) {
        localText += this->convertASTtoC(fnCall.params[i]);
        if(fnCall.returnType.base == TYPE_FUNC || i != (int)fnCall.params.size() - 1) localText += ",";
      }
      if(fnCall.returnType.base == TYPE_FUNC) {
        for(int i = 0; i < (int)args.size(); i++)
          localText += getVariableName(args[i]) + (i != (int)args.size() - 1 ? "," : ");\n");
        localText += "}\n";
        this->prevDefinitions += localText;
      }
      else
        text = localText + ")";
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
    case OP_MATCH:
    {
        OperationMatch match = op->match;
        Type tagUnionType = match.expr->get_data()->tk.type;
        size_t taggedUnionId = getTaggedUnionOrTuppleId(tagUnionType);
        string tagUnion = this->convertASTtoC(match.expr);
        text = "switch((" + tagUnion + ").type) {\n";
        for(size_t i = 0; i < match.castedVars.size(); i++) {
          Variable v = match.castedVars[i];
          size_t curId = fromTaggedUnionIdGetTypeId(taggedUnionId, v.type);
          text += "  case TYPE_" + to_string(curId) + ":\n{\n";
          text += convertToCVariable(v) + " = (" + tagUnion + ").FIELD_" + to_string(curId) + ";\n"; 
          defsGen.scopes.push_back(Scope(SCOPE_MATCH_BRANCH));
          defsGen.addDefinition(v);

          for(auto op : match.branches[i])
            text += "    " + this->convertASTtoC(op);
          text += "}\n  break;\n";

          defsGen.popDefinitions();
        }
        text += "}\n";
    }
    break;
    case OP_ELEM_LIST:
    {
      OperationElemList elemList = op->elemList;
      if(elemList.elemType.base == TYPE_COMPOUND) {
        size_t id = getTaggedUnionOrTuppleId(elemList.elemType);
        text = "(struct tupple" + to_string(id) + ")";
      }
      text += "{";
      for(int i = 0; i < (int)elemList.values.size(); i++) {
        text += this->convertASTtoC(elemList.values[i]);
        if(i != (int)elemList.values.size() - 1) text += ",";
      }
      text += "}";
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
        case TK_DOT:
        {
          AnalyzedParsedFile *child = ast->get_neighbor(CHILD(1));
          AnalyzedParsedFile *child2 = ast->get_neighbor(CHILD(2));
          Type t = getTypeFromAnalyzedParsedFile(child);
          Variable *v;
          if((v = defsGen.findDefinition(child->get_data()->tk.tk->text)) == NULL) {
            printf("Error: trying to access a field of a variable that does not exist.\n");
            printf("At line: %d, column: %d\n", (int)child->get_data()->pos.l, (int)child->get_data()->pos.e);
            exit(1);
          }
          if(t.base == TYPE_COMPOUND && child2->get_data()->type == OP_TOKEN && child2->get_data()->tk.tk->type == TK_INT) {
            //the field bounds were checked on analyzer
            size_t field = stoi(child2->get_data()->tk.tk->text);
            text = this->convertASTtoC(child) + ".FIELD_" + to_string(field);
          }
          else
            text = this->convertASTtoC(child) + "." + this->convertASTtoC(ast->get_neighbor(CHILD(2)));
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

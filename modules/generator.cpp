#include "generator.h"
#include "utils.h"

DefinitionsHandler defsGen;

string getVariableName(Variable v) {
  if(v.name != "main")
    return v.name + "_" + to_string(v.id);
  return v.name;
}

string Generator::typeAsCType(Type t) {
  switch(t.base) {
    case TYPE_INT:
      return "int";
    case TYPE_BYTE:
      return "char";
    case TYPE_NONE:
      return "void";
    case TYPE_REF:
    case TYPE_REF_VAR:
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
    case TYPE_COMPOUND:
    case TYPE_TAG_UNION:
    {
      size_t id = this->getTaggedUnionOrTupleId(t);
      return (t.base == TYPE_TAG_UNION ? "struct taggedUnion" : "struct tuple") + to_string(id);
    }
    break;
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
    case TYPE_REF_VAR:
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
      return "struct taggedUnion" + to_string(getTaggedUnionOrTupleId(v.type)) + " " + nameAndId;
    case TYPE_COMPOUND:
      return "struct tuple" + to_string(getTaggedUnionOrTupleId(v.type)) + " " + nameAndId;
    case TYPE_FUNC: //probably wont be needed
    default:
      printf("Error at convertToCVariable: this type is not implemented yet: %s\n", typeAsCType(v.type).c_str());
      exit(1);
      break;
  }
}

//returns the position of the type in the subtypes of the tagged union
//-1 if the type is the same as the tagged union
int Generator::fromTaggedUnionIdGetTypeId(size_t tagUnionId, Type t) {
  if(t.base == TYPE_UNKNOWN) {
    printf("Error: expected a type, but got unknown\n");
    exit(1);
  }
  if(t.base == TYPE_TAG_UNION) {
    if(!confirmType(&t, &this->definedTypes[tagUnionId])) {
      printf("Error: expected a tagged union %s, but got %s\n", this->definedTypes[tagUnionId].textType.c_str(), t.textType.c_str());
      exit(1);
    }
    return -1;
  }
  for(size_t i = 0; i < this->definedTypes[tagUnionId].subTypes.size(); i++) {
    if(confirmType(&t, &this->definedTypes[tagUnionId].subTypes[i])) return i;
  }
  printf("Error: could not find the type %s in the tagged union of type %s\n", t.textType.c_str(), this->definedTypes[tagUnionId].textType.c_str());
  exit(1);
}

size_t Generator::getTaggedUnionOrTupleId(Type t) {
  if(t.base != TYPE_TAG_UNION && t.base != TYPE_COMPOUND) {
    printf("Error: expected a tagged union, but got %s\n", typeAsCType(t).c_str());
    exit(1);
  }
  size_t index = 0; //the index of the defined type, so the name of the type is taggedUnion + index
  for(auto s : this->definedTypes) {
    if(confirmType(&t, &s)) return index;
    index++;
  }
  string text = (t.base == TYPE_TAG_UNION ? "struct taggedUnion" : "struct tuple") + to_string(index) + " {\n";
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
  string text = "";
  string returnType = typeAsCType(f.type.subTypes[f.type.subTypes.size() - 1]);
  string variableName = getVariableName(f);
  if(returnType.find('$') != string::npos) {
    string defReturnType = returnType;
    returnType = variableName + "_ret";
    defReturnType.replace(defReturnType.find('$'), 1, returnType);
    text = "typedef " + defReturnType + ";\n";
  }
  text += returnType + " " + variableName + "(";
  for(auto v : args) {
    text += convertToCVariable(v);
    text += v.id != args[args.size() - 1].id ? "," : "";
    defsGen.addDefinition(v);
  }
  text += ") {\n";
  return text;
}

string Generator::getValueForTaggedUnion(Type tagUnionType, AnalyzedParsedFile *value) {
  Type valueType = getTypeFromAnalyzedParsedFile(value);
  size_t id = getTaggedUnionOrTupleId(tagUnionType);
  int typeId = fromTaggedUnionIdGetTypeId(id, valueType);
  if(typeId == -1)
    return this->convertASTtoC(value);

  string idStr = to_string(typeId);
  return "(struct taggedUnion" + to_string(id) + ") {.type = TYPE_" + idStr + ", .FIELD_" + idStr + " = " + this->convertASTtoC(value) + "}";
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
      OperationFuncDef fnDef = op->funcDef;
      vector<Variable> prevDefinedVars = fnDef.defsFromPrevScopes;
      //if there are variables used inside this function that were not defined inside it
      //we need to pass a context to this function, a struct with the variables that were used
      string funcDef = createFunc(f, fnDef.args);

      if(prevDefinedVars.size() > 0) {
        string funcName = getVariableName(f);
        string contextType = "struct context_" + funcName;
        string context = contextType + " {\n";
        for(auto d : prevDefinedVars) {
          Variable v = d;
          if(v.mut) {
            v.type = Type{ .textType = "", .base = TYPE_REF_VAR, .subTypes = {d.type} };
            v.type.textType = typeAsCType(d.type);
          }
          string defineVar = "  " + convertToCVariable(v);
          string variableName = getVariableName(v);

          context += defineVar + ";\n";
          funcDef += defineVar + " = g_context_" + funcName + "." + variableName + ";\n";
          text += (v.mut ? "&": "") + variableName + ", "; // TODO: pass as reference, for variables can be changed
        }
        context += "};\n";
        context += contextType + " g_context_" + funcName + " = {};\n";
        text = "g_context_" + funcName + " = (" + contextType + "){" + text + "};\n";
        this->prevDefinitions += context;
      }

      for(auto op : fnDef.ops)
        funcDef += "  " + this->convertASTtoC(op);

      funcDef += "}\n";
      this->prevDefinitions += funcDef;
    }
    break;
    case OP_VAR_DEF:
    {
      OperationVarDef varDef = op->varDef;
      if(varDef.var.type.base == TYPE_FUNC && 
        (varDef.value->get_data()->type == OP_FUNC_DEF ||
        varDef.value->get_data()->type == OP_FUNC_CALL)
      ){
        defsGen.addDefinition(varDef.var);
        defsGen.scopes.push_back(new Scope(SCOPE_FUNC, funcScope()));      //start of function scope
        text = this->convertASTtoC(varDef.value);
        defsGen.popDefinitions();                         //end of function scope
      } else {
        if(varDef.var.type.base == TYPE_FUNC) {
          printf("When passing a function to another constant, use a pointer to a function! Try add an '#'.\n");
          printf("Error: type mismatch at line: %d, column: %d\n", (int)varDef.value->get_data()->pos.l, (int)varDef.value->get_data()->pos.e);
          exit(1);
        }
        Variable *v;
        if((v = defsGen.findDefinition(varDef.var.name)) != NULL && v->mut && varDef.var.mut) {
          string variableName = (defsGen.variablePassedFromContext(*v)) ? "(*" + getVariableName(*v) + ")" : getVariableName(*v);
          if(varDef.var.type.base == TYPE_TAG_UNION)
            text = variableName + " = " + this->getValueForTaggedUnion(varDef.var.type, varDef.value) + ";\n";
          else
            text = variableName + " = " + this->convertASTtoC(varDef.value) + ";\n";
        }
        else {
          if(varDef.leftHandAssignment->get_data()->type != OP_TOKEN)
             text = this->convertASTtoC(varDef.leftHandAssignment) + " = ";
          else {
             defsGen.addDefinition(varDef.var);
             text = convertToCVariable(varDef.var) + " = ";
          }
          string value = this->convertASTtoC(varDef.value);
          if(varDef.var.type.base == TYPE_TAG_UNION)
              text += this->getValueForTaggedUnion(varDef.var.type, varDef.value) + ";\n";
          else
             text += value + ";\n";
        }
      }
    }
    break;
    case OP_FUNC_CALL:
    {
      OperationFuncCall fnCall = op->funcCall;
      string caller = this->convertASTtoC(op->funcCall.func);
      string localText = "";
      //check if the call is by a function pointer
      bool isFuncPointerCall = fnCall.func->get_data()->type == OP_DEREF;

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
      AnalyzedParsedFile *func = fnCall.func;
      // partial function call with a function pointer
      // we will create a function as normal, but a global function pointer will be created
      // to store the function pointer, being used in the function call
      if(isFuncPointerCall && fnCall.returnType.base == TYPE_FUNC) {
        AnalyzedParsedFile *tmp = func;
        while(tmp->get_data()->type == OP_DEREF)
          tmp = tmp->get_data()->deref.expr;
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
      Type funcType = getTypeFromAnalyzedParsedFile(func);
      for(int i = 0; i < (int)fnCall.params.size(); i++) {
        if(funcType.subTypes[i].base == TYPE_TAG_UNION)
          localText += this->getValueForTaggedUnion(funcType.subTypes[i], fnCall.params[i]);
        else
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
        text = localText + (fnCall.returnType.base == TYPE_NONE ? ");\n" : ")");
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

      defsGen.scopes.push_back(new Scope(SCOPE_COND));
      for(auto op : cond.ops)
        text += "  " + this->convertASTtoC(op);
      text += "\n}\n";
    }
    break;
    case OP_LOOP:
    {
      OperationLoop loop = op->loop;
      if(loop.label != "")
        text = loop.label + "_continue:\n";
      text += "while(" + (loop.expr != NULL ? this->convertASTtoC(loop.expr) : "1") + ") {\n";
      defsGen.scopes.push_back(new Scope(SCOPE_LOOP, loopScope()));
      for(auto op : loop.ops)
        text += "  " + this->convertASTtoC(op);
      text += "}\n";
      if(loop.label != "")
        text += loop.label + "_break:\n";
    }
    break;
    case OP_MATCH:
    {
        OperationMatch match = op->match;
        Type tagUnionType = match.expr->get_data()->tk.type;
        size_t taggedUnionId = getTaggedUnionOrTupleId(tagUnionType);
        string tagUnion = this->convertASTtoC(match.expr);
        text = "switch((" + tagUnion + ").type) {\n";
        for(size_t i = 0; i < match.castedVars.size(); i++) {
          Variable v = match.castedVars[i];
          int curId = fromTaggedUnionIdGetTypeId(taggedUnionId, v.type);
          text += "  case TYPE_" + to_string(curId) + ":\n{\n";
          text += convertToCVariable(v) + " = (" + tagUnion + ").FIELD_" + to_string(curId) + ";\n"; 
          defsGen.scopes.push_back(new Scope(SCOPE_MATCH_BRANCH));
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
        size_t id = getTaggedUnionOrTupleId(elemList.elemType);
        text = "(struct tuple" + to_string(id) + ")";
      } else if(elemList.elemType.base == TYPE_NONE) {
        break; // WARN: maybe at some point we will need to return something here
      }
      text += "{";
      for(int i = 0; i < (int)elemList.values.size(); i++) {
        text += this->convertASTtoC(elemList.values[i]);
        if(i != (int)elemList.values.size() - 1) text += ",";
      }
      text += "}";
    }
    break;
    case OP_REF:
    {
      OperationRef ref = op->ref;
      text = "&" + this->convertASTtoC(ref.expr);
    }
    break;
    case OP_DEREF:
    {
      OperationDeref deref = op->deref;
      if(deref.offset != 0)
        text = this->convertASTtoC(deref.expr) + "[" + to_string(deref.offset) + "]";
      else
        text = "*" + this->convertASTtoC(deref.expr);

      if(deref.expr->get_data()->type == OP_DEREF && deref.expr->get_data()->deref.offset > deref.offset && deref.offset == 0)
        text = "(" + text + ")";
    }
    break;
    case OP_ACCESS_FIELD:
    {
      OperationAccessField acField = op->accessField;
      Type t = getTypeFromAnalyzedParsedFile(acField.root);
      string root = this->convertASTtoC(acField.root);
      if(t.base == TYPE_COMPOUND) {
        //the field bounds were checked on analyzer
        size_t field = stoi(acField.field->get_data()->tk.tk->text);
        text = (acField.root->get_data()->type == OP_DEREF ? "(" + root +")" : root) + ".FIELD_" + to_string(field);
      } else
        text = root + "." + this->convertASTtoC(acField.field);
    }
    break;
    case OP_TOKEN:
    {
      OperationToken tk = op->tk;
      switch(tk.tk->type) {
        case TK_BLOCK_BACK: text = "return " + this->convertASTtoC(ast->get_neighbor(CHILD(1))) + ";\n"; break;
        case TK_BLOCK_SKIP:
        case TK_BLOCK_STOP:
        {
          text = tk.tk->type == TK_BLOCK_SKIP ? "continue" : "break";
          if(ast->get_neighbors_size() > CHILD(1))
            text = "goto " + ast->get_neighbor(CHILD(1))->get_data()->tk.tk->text  + "_" + text;
          text += ";\n";
        }
        break;
        case TK_LOG_OR:
        case TK_LOG_AND:
        case TK_LOG_NOT:
        case TK_BIT_OR:
        case TK_BIT_AND:
        case TK_BIT_NOT:
        case TK_BIT_XOR:
        case TK_BIT_SHIFT_L:
        case TK_BIT_SHIFT_R:
        {
          vector<pair<string, TokenType>> texts = {
            {"||", TK_LOG_OR},
            {"&&", TK_LOG_AND},
            {"!", TK_LOG_NOT},
            {"|", TK_BIT_OR},
            {"&", TK_BIT_AND},
            {"~", TK_BIT_NOT},
            {"^", TK_BIT_XOR},
            {"<<", TK_BIT_SHIFT_L},
            {">>", TK_BIT_SHIFT_R},
          };
          for(auto t : texts) {
            if(t.second == tk.tk->type && t.second != TK_LOG_NOT && t.second != TK_BIT_NOT) {
              //two operands
              text = this->convertASTtoC(ast->get_neighbor(CHILD(1))) + " " + t.first + " " + this->convertASTtoC(ast->get_neighbor(CHILD(2)));
              break;
            }
            else if(t.second == tk.tk->type) {
              //one operand
              text = t.first + this->convertASTtoC(ast->get_neighbor(CHILD(1)));
              break;
            }
          }
        } break;

        //operations with two operands
        case TK_NUM_ADD:
        case TK_NUM_SUB:
        case TK_NUM_MUL:
        case TK_NUM_DIV:
        case TK_NUM_MOD:
        case TK_LOG_EQ:
        case TK_LOG_NE:
        case TK_LOG_GE:
        case TK_LOG_LE:
        case TK_LOG_GT:
        case TK_LOG_LT:
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
        case TK_BLOCK_EMBED:
        {
              string c_code = ast->get_neighbor(RIGHT_LINK)->get_data()->tk.tk->text;
              c_code = c_code.substr(1, c_code.size() - 2) + '\n';
              while(c_code.find('$') != string::npos) {
                size_t pos = c_code.find('$');
                if(c_code[pos + 1] != '{') {
                  printf("Error: expected a '{' after the '$' at line: %d, column: %d\n", (int)ast->get_data()->pos.l, (int)ast->get_data()->pos.e);
                  exit(1);
                }
                size_t end = c_code.find('}', pos);
                string varName = c_code.substr(pos + 2, end - pos - 2);
                Variable v = *defsGen.findDefinition(varName); //wont exist any null return from findDefinition, all of them were solved at analyzer
                c_code.replace(pos, end - pos + 1,
                       defsGen.variablePassedFromContext(v) && v.mut ? "(*" + getVariableName(v) + ")" : getVariableName(v)
                );
              }
              text = c_code;
        }
        break;
        case TK_NAME:
        {
          //wont exist any null return from findDefinition, all of them were solved at analyzer, i think.
          Variable v = *defsGen.findDefinition(tk.tk->text);
          text = defsGen.variablePassedFromContext(v) && v.mut ? "(*" + getVariableName(v) + ")" : getVariableName(v);
        } break;
        default:
          text = tk.tk->text;
      }
    }
    break;
    default:
      printf("not implemented yet:\n");
      printAnalyzerParsedFile(ast, "  ");
      exit(1);
  }
  return text;
}

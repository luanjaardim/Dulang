#include "utils.h"
#include "node.h"
#include "tokenizer.h"
#include "grammar.h"

enum BaseType {
    //TYPE_FLOAT,
    TYPE_INT,
    TYPE_BYTE,
    TYPE_REF,
    TYPE_FUNC,       //function, WARN: do not change it's position in the enum
    TYPE_TAG_UNION,  //enum
    TYPE_COMPOUND,   //struct
    TYPE_NONE,
    TYPE_USER_DEFINED,
    TYPE_UNKNOWN,
};

struct Type {
    string textType;
    BaseType base;
    vector<Type> subTypes;
};

//variables, constants and functions
struct Variable {
    size_t id;
    string name;
    bool mut = false;   //mutable
    Position pos;
    Type type;
};

struct Operation;
typedef Node<Operation *> AnalyzedParsedFile;

struct OperationToken {
    Token *tk;
    Type type;
};

struct OperationFuncDef {
    vector<Variable> args;
    vector<Variable> defsFromPrevScopes; //definitions from other scopes that were used in this scope
    Type type;
    vector<AnalyzedParsedFile *> ops;
};

struct OperationVarDef {
    Variable var;
    AnalyzedParsedFile *value;
};

struct OperationTypeDef {
    Variable var; //has the name to alias the type
};

struct OperationCond {
    enum CondType {
        NONE = 0, IF = 1, ELSE = 2, ELSE_IF = 3
    } type;
    AnalyzedParsedFile *expr;
    vector<AnalyzedParsedFile *> ops;
};

struct OperationLoop {
    AnalyzedParsedFile *expr;
    vector<AnalyzedParsedFile *> ops;
};

struct OperationFuncCall {
    AnalyzedParsedFile *func;
    Type returnType;
    vector<AnalyzedParsedFile *> params;
};

struct OperationMatch {
    AnalyzedParsedFile *expr;
    vector<Variable> castedVars;
    vector<vector<AnalyzedParsedFile *>> branches;
};

struct OperationElemList {
    enum {
        ARRAY, TUPPLE
    } type;
    Type elemType;
    vector<AnalyzedParsedFile *> values;
};

enum OperationType {
    OP_TOKEN, //default operation, only one token
    OP_FUNC_DEF, //function definition
    OP_VAR_DEF, //variable definition
    OP_TYPE_DEF,
    OP_COND, //if, else if and else
    OP_LOOP, //while and loop
    OP_FUNC_CALL,
    OP_MATCH,
    OP_ELEM_LIST, //for array, struct
};

struct Operation {
    Position pos;
    OperationType type;
    union {
        OperationToken tk;
        OperationFuncDef funcDef;
        OperationVarDef varDef;
        OperationTypeDef typeDef;
        OperationCond cond;
        OperationLoop loop;
        OperationFuncCall funcCall;
        OperationMatch match;
        OperationElemList elemList;
    };

    Operation(OperationToken tk, Position p) : pos(p), type(OP_TOKEN), tk(tk) {}
    Operation(OperationFuncDef funcDef, Position p) : pos(p), type(OP_FUNC_DEF), funcDef(funcDef) {}
    Operation(OperationTypeDef typeDef, Position p) : pos(p), type(OP_TYPE_DEF), typeDef(typeDef) {}
    Operation(OperationVarDef varDef, Position p) : pos(p), type(OP_VAR_DEF), varDef(varDef) {}
    Operation(OperationCond cond, Position p) : pos(p), type(OP_COND), cond(cond) {}
    Operation(OperationLoop loop, Position p) : pos(p), type(OP_LOOP), loop(loop) {}
    Operation(OperationFuncCall funcCall, Position p) : pos(p), type(OP_FUNC_CALL), funcCall(funcCall) {}
    Operation(OperationMatch match, Position p) : pos(p), type(OP_MATCH), match(match) {}
    Operation(OperationElemList elemList, Position p) : pos(p), type(OP_ELEM_LIST), elemList(elemList) {}
};

AnalyzedParsedFile *analyzeParsedFile(ParsedFile *tokens);
Type analyzeType(ParsedFile *tokens);
bool confirmType(Type *t, Type *s);
void printAnalyzerParsedFile(AnalyzedParsedFile *parsedFile, string tab);
Type getTypeFromAnalyzedParsedFile(AnalyzedParsedFile *apf);

enum ScopeType {
    SCOPE_GLOBAL,
    SCOPE_FUNC,
    SCOPE_LOOP,
    SCOPE_COND,
    SCOPE_MATCH_BRANCH,
};

struct Scope {
    AnalyzedParsedFile *scopeMainNode;
    ScopeType type;
    Type returnType;
    vector<Variable> defs = {};
    vector<Variable> defsFromPrevScope = {}; //definitions from other scopes that were used in this scope
    Scope(ScopeType type) : type(type) { }
};

struct DefinitionsHandler {
    vector<Scope> scopes = {Scope(SCOPE_GLOBAL)};

    void addDefinition(Variable v) { scopes.back().defs.push_back(v); }
    void popDefinitions() { scopes.pop_back(); }
    Variable *getLastDefinition() {
        for(int i = (int)scopes.size() - 1; i >= 0; i--) {
            if(scopes[i].defs.size() > 0) {
                return &scopes[i].defs.back();
            }
        }
        printf("Trying to get last definition that does not exist"); 
        exit(1);
    }
    Variable *findDefinition(string name) {
        for( int i = (int)scopes.size() - 1; i >= 0; i--) {
            for(int j = (int)scopes[i].defs.size() - 1; j >= 0; j--) {
                if(scopes[i].defs[j].name == name) {
                    //add the definition to the next scopes, as used by them, this will be part of the context of the fuction
                    if(i > 0) //do not add if from global scope, and only add if it is a function
                        for(int k = i + 1; k < (int)scopes.size(); k++) {
                            if(scopes[k].type == SCOPE_FUNC) {
                                bool add = true;
                                for( auto v : scopes[k].defsFromPrevScope) //already added
                                    if(v.id == scopes[i].defs[j].id) add = false;
                                if(add)
                                    scopes[k].defsFromPrevScope.push_back(scopes[i].defs[j]);
                            }
                        }
                    return &scopes[i].defs[j];
                }
            }
        }
        return NULL;
    }
    void printDefinitions() {
        for(int i = 0; i < (int)scopes.size(); i++) {
            printf("Scope %d\n", i);
            for(int j = 0; j < (int)scopes[i].defs.size(); j++) {
                printf("  %s\n", scopes[i].defs[j].name.c_str());
            }
        }
    }
};

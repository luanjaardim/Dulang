#include "utils.h"
#include "node.h"
#include "tokenizer.h"
#include "grammar.h"

enum BaseType {
    //TYPE_FLOAT,
    TYPE_INT,
    TYPE_BYTE,
    TYPE_REF,
    TYPE_REF_VAR,
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
    // this will always be NULL, unless it is an assignment
    AnalyzedParsedFile *leftHandAssignment = NULL; //left hand side of the assignment
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
    string label;
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
        ARRAY, TUPLE
    } type;
    Type elemType;
    vector<AnalyzedParsedFile *> values;
};

struct OperationDeref {
    Type type;
    AnalyzedParsedFile *expr;
    int offset = 0;
};

struct OperationRef {
    Type type; //TYPE_REF or TYPE_REF_VAR
    AnalyzedParsedFile *expr;
};

struct OperationAccessField {
    Type type;
    AnalyzedParsedFile *root, *field;
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
    OP_DEREF, //dereference a pointer
    OP_REF, //returns a reference to something
    OP_ACCESS_FIELD, //access a field of struct
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
        OperationDeref deref;
        OperationRef ref;
        OperationAccessField accessField;
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
    Operation(OperationDeref deref, Position p) : pos(p), type(OP_DEREF), deref(deref) {}
    Operation(OperationRef ref, Position p) : pos(p), type(OP_REF), ref(ref) {}
    Operation(OperationAccessField accessField, Position p) : pos(p), type(OP_ACCESS_FIELD), accessField(accessField) {}
};

AnalyzedParsedFile *analyzeFunc(ParsedFile *tokens);
AnalyzedParsedFile *analyzeTypeDef(ParsedFile *tokens);
AnalyzedParsedFile *analyzeVar(ParsedFile *tokens);
AnalyzedParsedFile *analyzeCond(ParsedFile *tokens);
AnalyzedParsedFile *analyzeLoop(ParsedFile *tokens);
AnalyzedParsedFile *analyzeFuncCall(ParsedFile *tokens);
AnalyzedParsedFile *analyzeMatch(ParsedFile *tokens);
AnalyzedParsedFile *analyzeListOfElements(ParsedFile *tokens);
AnalyzedParsedFile *analyzeDeref(ParsedFile *tokens);
AnalyzedParsedFile *analyzeRef(ParsedFile *tokens);
AnalyzedParsedFile *analyzeAccessField(ParsedFile *tokens);
AnalyzedParsedFile *analyzeToken(ParsedFile *tokens);
AnalyzedParsedFile *analyzeParsedFile(ParsedFile *tokens);

Type analyzeType(ParsedFile *tokens);
Variable analyzeParseType(ParsedFile *tokens);
bool confirmType(Type *t, Type *s);
string typeString(Type t);
Type getTypeFromAnalyzedParsedFile(AnalyzedParsedFile *apf);
void printAnalyzerParsedFile(AnalyzedParsedFile *parsedFile, string tab);

enum ScopeType {
    SCOPE_GLOBAL,
    SCOPE_FUNC,
    SCOPE_LOOP,
    SCOPE_COND,
    SCOPE_MATCH_BRANCH,
};

struct funcScope {
    vector<Variable> defsFromPrevScopes; //definitions from other scopes that were used in this scope
    funcScope() {
        defsFromPrevScopes = {};
    }
};
struct loopScope {
    string label;
    loopScope() {
        label = "";
    }
};

struct Scope {
    ScopeType type;
    Position pos;
    Type returnType = { .textType = "none", .base = TYPE_NONE, .subTypes = {} };
    vector<Variable> defs = {};
    union {
        funcScope func;
        loopScope loop;
    };
    // Scope(ScopeType type, Position pos) : type(type), pos(pos) { }
    // Scope(ScopeType type, funcScope func, Position pos) : type(type), func(func), pos(pos) { }
    // Scope(ScopeType type, loopScope loop, Position pos) : type(type), loop(loop), pos(pos) { }
    Scope(ScopeType type) : type(type) { }
    Scope(ScopeType type, funcScope func) : type(type), func(func) { }
    Scope(ScopeType type, loopScope loop) : type(type), loop(loop) { }
    ~Scope() {}
};

struct DefinitionsHandler {
    ~DefinitionsHandler() {
        for(auto s : scopes) delete s;
    }
    vector<Scope *> scopes = {new Scope(SCOPE_GLOBAL)};

    void addDefinition(Variable v) { scopes.back()->defs.push_back(v); }
    void popDefinitions() {
        ScopeType type = scopes.back()->type;
        Type returnType = scopes.back()->returnType;
        scopes.pop_back();
        if(scopes.size() > 1 && type != SCOPE_FUNC) //do not pass the return type back if it is the global scope
            scopes.back()->returnType = returnType;
    }
    Variable *getLastDefinition() {
        for(int i = (int)scopes.size() - 1; i >= 0; i--) {
            if(scopes[i]->defs.size() > 0) {
                return &scopes[i]->defs.back();
            }
        }
        printf("Trying to get last definition that does not exist"); 
        exit(1);
    }
    Variable *findDefinition(string name) { 
        int scope, index;
        return findDefinitionAux(name, &scope, &index);
    }
    Variable *findDefinitionAux(string name, int *scope, int *index) {
        for(int i = (int)scopes.size() - 1; i >= 0; i--) {
            for(int j = (int)scopes[i]->defs.size() - 1; j >= 0; j--) {
                if(scopes[i]->defs[j].name == name) {
                    //add the definition to the next scopes, as used by them, this will be part of the context of the fuction
                    if(i > 0) //do not add if from global scope, and only add if it is a function
                    // TODO: only check this if in analyzer
                        for(int k = i + 1; k < (int)scopes.size(); k++) {
                            if(scopes[k]->type == SCOPE_FUNC) {
                                bool add = true;
                                for( auto v : scopes[k]->func.defsFromPrevScopes) //already added
                                    if(v.id == scopes[i]->defs[j].id) add = false;
                                if(add)
                                    scopes[k]->func.defsFromPrevScopes.push_back(scopes[i]->defs[j]);
                            }
                        }
                    *scope = i;
                    *index = j;
                    return &scopes[i]->defs[j];
                }
            }
        }
        return NULL;
    }
    bool variablePassedFromContext(Variable v) {
        int scope, index;
        findDefinitionAux(v.name, &scope, &index);
        if(scope == 0) return false;
        // if we find an function, that it was defined in a outter function, therefore passed from the context
        for(int i = scope + 1; i < (int)scopes.size(); i++) {
            if(scopes[i]->type == SCOPE_FUNC) return true;
        }
        return false;
    }
    void printDefinitions() {
        for(int i = 0; i < (int)scopes.size(); i++) {
            printf("Scope %d\n", i);
            for(int j = 0; j < (int)scopes[i]->defs.size(); j++) {
                printf("  %s\n", scopes[i]->defs[j].name.c_str());
            }
        }
    }
};

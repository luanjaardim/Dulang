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

enum OperationType {
    OP_TOKEN, //default operation, only one token
    OP_FUNC_DEF, //function definition
    OP_VAR_DEF, //variable definition
    OP_TYPE_DEF,
    OP_COND, //if, else if and else
    OP_LOOP, //while and loop
    // TODO: Add function call operation and type definitions
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
    };

    Operation(OperationToken tk, Position p) : pos(p), type(OP_TOKEN), tk(tk) {}
    Operation(OperationFuncDef funcDef, Position p) : pos(p), type(OP_FUNC_DEF), funcDef(funcDef) {}
    Operation(OperationTypeDef typeDef, Position p) : pos(p), type(OP_TYPE_DEF), typeDef(typeDef) {}
    Operation(OperationVarDef varDef, Position p) : pos(p), type(OP_VAR_DEF), varDef(varDef) {}
    Operation(OperationCond cond, Position p) : pos(p), type(OP_COND), cond(cond) {}
    Operation(OperationLoop loop, Position p) : pos(p), type(OP_LOOP), loop(loop) {}
};

AnalyzedParsedFile *analyzeParsedFile(ParsedFile *tokens);
Type analyzeType(ParsedFile *tokens);
void printAnalyzerParsedFile(AnalyzedParsedFile *parsedFile, string tab);

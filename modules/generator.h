#include "analyzer.h"

enum ScopeType {
    SCOPE_GLOBAL,
    SCOPE_FUNC,
    SCOPE_LOOP,
    SCOPE_COND,
};

struct Scope {
    ScopeType type;
    vector<Variable> defs;
    Scope(ScopeType type) : type(type) { }
};

struct Generator {
    Generator() {};
    vector<Scope> scopes = {Scope(SCOPE_GLOBAL)};

    void addDefinition(Variable v) { scopes.back().defs.push_back(v); }
    void popDefinitions() { scopes.pop_back(); }
    Variable getLastDefinition() {
        for(int i = (int)scopes.size() - 1; i >= 0; i--) {
            if(scopes[i].defs.size() > 0) {
                return scopes[i].defs.back();
            }
        }
        printf("Trying to get last definition that does not exist"); 
        exit(1);
    }
    Variable findDefinition(string name, Position err_pos) {
        Variable v;
        bool found = false;
        for( int i = (int)scopes.size() - 1; i >= 0 && !found; i--) {
            for(int j = (int)scopes[i].defs.size() - 1; j >= 0 && !found; j--) {
                if(scopes[i].defs[j].name == name) {
                    v = scopes[i].defs[j];
                    found = true;
                }
            }
        }
        if(!found) {
            printf("Variable of name: %s, is not defined. At line: %d and col: %d", name.c_str(), (int)err_pos.l, (int)err_pos.e);
            exit(1);
        } else return v;
    }
    void printDefinitions() {
        for(int i = 0; i < (int)scopes.size(); i++) {
            printf("Scope %d\n", i);
            for(int j = 0; j < (int)scopes[i].defs.size(); j++) {
                printf("  %s\n", scopes[i].defs[j].name.c_str());
            }
        }
    }
    string convertASTtoC(AnalyzedParsedFile *ast);
    string createFunc(Variable f, vector<Variable> args);
};

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
    void popDefinition() { scopes.pop_back(); }
    Variable getLastDefinition() {
        if(!this->scopes.empty() && !this->scopes.back().defs.empty()) 
            return this->scopes.back().defs.back();
        else { printf("Trying to get last definition that does not exist"); exit(1); }
    }
    Variable findDefinition(string name, Position pos) {
        Variable v;
        bool found = false;
        for( int i = scopes.size() - 1 && !found; i >= 0; i ++) {
            for(int j = scopes[i].defs.size() - 1 && !found; j >= 0; j ++) {
                if(scopes[i].defs[j].name == name) {
                    v = scopes[i].defs[j];
                    found = true;
                }
            }
        }
        if(!found) {
            printf("Variable of name: %s, is not defined. At line: %d and col: %d", name.c_str(), (int)pos.l, (int)pos.e);
            exit(1);
        } else return v;
    }
    string convertASTtoC(AnalyzedParsedFile *ast);
};



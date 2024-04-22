#include "analyzer.h"

struct Generator {
    Generator() {};
    string convertASTtoC(AnalyzedParsedFile *ast);
    string createFunc(Variable f, vector<Variable> args);
};

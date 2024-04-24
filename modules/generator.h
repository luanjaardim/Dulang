#include "analyzer.h"

struct Generator {
    string prevDefinitions = "";
    Generator() {};
    void translateFile(ParsedFile *ast, string fileName) {
        string text = "";
        ofstream out(fileName.c_str());
        for(int i = CHILD(1); i < (int)ast->get_neighbors_size(); i++) {
            if(ast->get_neighbor(i) == NULL) continue;
            AnalyzedParsedFile *node = analyzeParsedFile(ast->get_neighbor(i));
            printAnalyzerParsedFile(node, "");
            text = prevDefinitions + this->convertASTtoC(node);
            prevDefinitions = "";
            out << text;
        }
        printf("File %s successfully generated!\n", fileName.c_str());
        out.close();
    }
    string convertASTtoC(AnalyzedParsedFile *ast);
    string createFunc(Variable f, vector<Variable> args);
};

#include "utils.h"
#include "tokenizer.h"
#include "node.h"

// struct PatternStep {
//     string key;
//     Position start, end;
//     PatternStep(string key, Position start, Position end) : key(key), start(start), end(end) {}
//     ~PatternStep() {}
// };
//
// struct PatternSteps {
//     string parentPatternKey;
//     size_t patternIdx;
//     vector<PatternStep> steps;
//     PatternSteps(string key) : parentPatternKey(key) {}
//     ~PatternSteps() {}
// };

struct Element;

enum ElementType {
    KEY,
    VALUE,
    TEXT,
    INNER_ELEMENT,
    OPTIONAL,
    LIST,
};

struct ElementValue {
    TokenType type;
    ElementValue(TokenType type) : type(type) {}
};

// the text of this element is a key on the grammar, and it must be replaced by some other pattern, <text>
struct InnerElement {
    string elem;
    InnerElement(string elem) : elem(elem) {}
};

// the text of this element is an optional sentence, here we can have a sequence of elements, ?(text)
struct OptionalElement {
    vector<Element *> elements;
};

// a list of elements, separated by a separator, [text:'separator']
struct ElementList {
    Element *e, *separator;
    ElementList(Element *e, Element *separator) : e(e), separator(separator) {}
};

struct Element {
    ElementType type;
    union {
        ElementValue value;
        InnerElement innerElement;
        OptionalElement optionalElement;
        ElementList list;
    };
    Element(ElementValue value) : type(VALUE), value(value) {}
    Element(InnerElement innerElement) : type(INNER_ELEMENT), innerElement(innerElement) {}
    Element(OptionalElement optionalElement) : type(OPTIONAL), optionalElement(optionalElement) {}
    Element(ElementList list) : type(LIST), list(list) {}
    ~Element() {}
};

struct Rule {
    vector<Element *> elements;
    Rule() {}
};

typedef Node<Token *> ParsedFile;

struct Grammar {
    string filePath = "grammar";
    ifstream file;
    map<string, vector<Rule>> rules; //grammar rules for every key
    map<string, set<string>> firsts; //firsts for every key
    // vector<PatternStep> steps;

    Grammar() {
        file.open(filePath);
        if(!file.is_open()) {
            file.open("$HOME/.dir_dulang");
            if(!file.is_open()) {
                cerr << "Error: could find the path to Dulang repository." << endl;
                exit(1);
            }
            getline(file, filePath);
            file.close();
            filePath += "/grammar";
            file.open(filePath);
            if(!file.is_open()) {
                cerr << "Error: could not open file " << filePath << endl;
                exit(1);
            }
            file.close();
        }
        this->extractPatterns(new Lexer(this->filePath.c_str()));
        this->printPatterns();
        this->calculateFirsts();
    }
    ~Grammar() { }
    void loadGrammar(Grammar *gm);
    void extractPatterns(Lexer *lexer);
    void calculateFirsts();
    // ParsedFile *parseTokenizedFile(TokenizedFile *tf) {
    //     size_t lastLine = tf->lines.size() - 1, lastLineSize = tf->lines[lastLine]->tokens.size();
    //     return parseStep(tf, PatternStep("root", Position(0, 0), Position(lastLine, lastLineSize)));
    // }
    void printPatterns();

// private:
//     ParsedFile *parseStep(TokenizedFile *tf, PatternStep ps);
};

// bool handleElementType(
//   TokenizedFile *tf, 
//   Pattern p,
//   Element *e,
//   PatternSteps *steps,
//   size_t elem_idx, 
//   Position *end
// );
//
// Position findStartOfNextElement(
//   TokenizedFile *tf,
//   Pattern p,
//   Element *nextElem,
//   PatternSteps *steps,
//   size_t nextElemIdx,
//   Position *end
// );

void printAST(ParsedFile *ast, string tab);
void printType(Element *e, size_t tab);

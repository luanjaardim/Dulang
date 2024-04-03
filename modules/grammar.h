#include "utils.h"
#include "tokenizer.h"

struct PatternStep {
    string key;
    size_t line, start, end;
    PatternStep(string key, size_t line, size_t start, size_t end) 
        : key(key), line(line), start(start), end(end) {}
    ~PatternStep() {}
};

struct PatternSteps {
    string parentPatternKey;
    size_t patternIdx;
    vector<PatternStep> steps;
    PatternSteps() : parentPatternKey("") {}
    ~PatternSteps() {}
};

struct Element;

enum ElementType {
    KEY,
    VALUE,
    TEXT,
    INNER_ELEMENT,
    OPTIONAL,
    LIST,
};

enum ElementValueType {
    VAL_NUMBER,
    VAL_CHAR,
    VAL_STRING,
    VAL_NAME,
    VAL_INDENT,
    VAL_NEW_LINE,
    VAL_BLOCK,
};
// uppercase text, it represents a type of a token: NUMBER, STRING, NAME, etc, or a marker: INDENT, DEDENT, NEW_LINE
struct ElementValue {
    ElementValueType type;
    ElementValue(ElementValueType type) : type(type) {}
};

// the text of this element is a hardcoded string, the word written itself, "text"
struct ElementText {
    string text;
    ElementText(string text) : text(text) {}
};

// the text of this element creates a key on the grammar, text:
struct ElementKey {
    string key;
    ElementKey(string key) : key(key) {}
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
        ElementKey key;
        ElementValue value;
        ElementText text;
        InnerElement innerElement;
        OptionalElement optionalElement;
        ElementList list;
    };
    Element(ElementKey key) : type(KEY), key(key) {}
    Element(ElementValue value) : type(VALUE), value(value) {}
    Element(ElementText text) : type(TEXT), text(text) {}
    Element(InnerElement innerElement) : type(INNER_ELEMENT), innerElement(innerElement) {}
    Element(OptionalElement optionalElement) : type(OPTIONAL), optionalElement(optionalElement) {}
    Element(ElementList list) : type(LIST), list(list) {}
    ~Element() {}
};

struct Pattern {
    vector<Element *> elements;
    Pattern() {}
};

struct Grammar {
    const string filePath = "grammar";
    map<string, vector<Pattern>> patterns;
    vector<PatternStep> steps;

    Grammar() {
        loadGrammar(this);
    }
    ~Grammar() { }
    void loadGrammar(Grammar *gm);
    void extractPatterns(TokenizedFile *tf);
    bool parseFile(TokenizedFile *tf, PatternStep ps);
    void printPatterns();
};

void printType(Element *e, size_t tab);
bool handleElementType(struct handleElemType h);

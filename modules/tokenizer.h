#ifndef TOK_H_
#define TOK_H_

#include "utils.h"

typedef struct Token {
    size_t id;
    size_t l, c; //line and column
    string text;
    TokenType type;

    Token(string text, size_t id, TokenType type, size_t l, size_t c) : id(id), l(l), c(c), text(text), type(type) {}
} Token;

typedef struct TokenizedLine {
    vector<Token *> tokens;

    TokenizedLine() {}
} TokenizedLine;

typedef struct TokenizedFile {
    Position pos;
    vector<TokenizedLine *> lines;

    TokenizedFile() : pos(Position(0, 0)) {}
} TokenizedFile;

typedef struct FileReader {
    ifstream file;
    string content;
    string word;
    size_t currPos, currEndOfWord, currLine, currCol; //specific to the current word
} FileReader;

void printTokenizedFile(TokenizedFile p);
TokenizedFile *readToTokenizedFile(const char *file);
void destroyTokenizdFile(TokenizedFile *tp);
TokenizedFile *cloneTokenizedFile(const TokenizedFile tf);
Token *currToken(TokenizedFile tf);
Token *nextToken(TokenizedFile *tf, size_t n);
Token *nextLineToken(TokenizedFile *tf, size_t n);
Token *peekToken(TokenizedFile tf, size_t n);
Token *peekLineToken(TokenizedFile tf, size_t n);
Token *returnToken(TokenizedFile *tf, size_t n);
Token *returnLineToken(TokenizedFile *tf, size_t n);
Token *peekBackToken(TokenizedFile tf, size_t n);
Token *peekBackLineToken(TokenizedFile tf, size_t n);
int advanceLineTokenizdFile(TokenizedFile *tf);

struct endOfBlock {
  size_t lastId, lastLine;
};
struct endOfBlock endOfCurrBlock(TokenizedFile tf);

#define min(a, b) (a) < (b) ? (a) : (b)

#endif // TOK_H_

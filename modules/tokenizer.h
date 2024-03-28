#ifndef TOK_H_
#define TOK_H_

#include "utils.h"

typedef struct {
    TokenType type;
    TokenPrecedence precedence;
} TkInfo;

typedef struct Token {
    size_t id;
    size_t l, c; //line and column
    string text;
    TokenType type;
    TokenPrecedence precedence;
} Token;

typedef struct TokenizedLine {
    vector<Token> tokens;
} TokenizedLine;

typedef struct TokenizedFile {
    size_t currLine, currElem; //used for navigation
    vector<TokenizedLine> lines;
} TokenizedFile;

typedef struct FileReader {
    ifstream file;
    string content;
    string word;
    size_t currPos, currEndOfWord, currLine, currCol; //specific to the current word
} FileReader;

void printTokenizedFile(TokenizedFile p);
TokenizedFile readToTokenizedFile(const char *file);
void destroyTokenizdFile(TokenizedFile *tp);
TokenizedFile cloneTokenizedFile(const TokenizedFile tf);
Token *currToken(TokenizedFile tf);
Token *nextToken(TokenizedFile *tf);
Token *peekToken(TokenizedFile tf);
Token *returnToken(TokenizedFile *tf);
Token *peekBackToken(TokenizedFile tf);
int advanceLineTokenizdFile(TokenizedFile *tf);

struct endOfBlock {
  size_t lastId, lastLine;
};
struct endOfBlock endOfCurrBlock(TokenizedFile tf);

#define min(a, b) (a) < (b) ? (a) : (b)

#endif // TOK_H_

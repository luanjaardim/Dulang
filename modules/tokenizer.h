#ifndef TOK_H_
#define TOK_H_

#include "utils.h"

typedef struct Token {
    size_t id;
    TokenType type;
    Position pos;
    string text;
} Token;

typedef struct Lexer {
    size_t tokenId = 0;
    ifstream file;
    string content;
    string curWord;
    Position pos;
    char curChar;
    size_t fileIndex = 0;

    const string doubleEspChars = ";=-+*/%<!>:"; 
    const vector<string> possibleCombinations = {
      "==", "!=", ">=", "<=", "++", "--", "+=", "-=", "*=", "/=", "%=", "<<", ">>", "<>", "->", "<-", "=>", "::", ";;"
    };
    const string espChars = " \t\n@#&|?,.()[]{}\'\"`";
    const string specialChars = doubleEspChars + espChars + '$'; //'$' for comments

    Lexer(const char *file) : file(file), pos(Position(1, 1)) {
      if(!this->file.is_open()) {
          cerr << "Error: could not open file " << file << endl;
          exit(1);
      }
      content = string((std::istreambuf_iterator<char>(this->file)), std::istreambuf_iterator<char>());
      curChar = content.empty() ? '\0' : content[0];
      fileIndex = 1;
    }
    void consumeWhiteSpaces();
    void consumeComment();
    char nextChar();
    char peekChar();
    Token nextWord();
    void printToken(Token token);
} Lexer;

#endif // TOK_H_

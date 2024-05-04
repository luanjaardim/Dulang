#ifndef TOK_IMPL_
#define TOK_IMPL_

#include "tokenizer.h"
#include "utils.h"

struct Symb {
  const char *symbol;
  TokenType tokenType;
};

#define LEN_BUILTIN_WORDS (COUNT_OF_TK_TYPES - (MARKER + 1))
static const struct Symb builtinWords[LEN_BUILTIN_WORDS] = {
  //numeric operations
  {"+",     TK_NUM_ADD},
  {"-",     TK_NUM_SUB},
  {"/",     TK_NUM_DIV},
  {"*",     TK_NUM_MUL},
  {"%",     TK_NUM_MOD},
  //logic operations
  {"not",   TK_LOG_NOT},
  {"or",    TK_LOG_OR},
  {"and",   TK_LOG_AND},
  {"==",    TK_LOG_EQ},
  {"!=",    TK_LOG_NE},
  {">=",    TK_LOG_GE},
  {"<=",    TK_LOG_LE},
  {">",     TK_LOG_GT},
  {"<",     TK_LOG_LT},
  //bitwise TK_operations
  {"bnot",  TK_BIT_NOT},
  {"bor",   TK_BIT_OR},
  {"band",  TK_BIT_AND},
  {"shl",   TK_BIT_SHIFT_L},
  {"shr",   TK_BIT_SHIFT_R},
  {"bxor",  TK_BIT_XOR},
  //types
  {"byte",  TK_TYPE_BYTE},
  {"ubyte", TK_TYPE_UBYTE}, 
  {"int",   TK_TYPE_INT},
  {"uint",  TK_TYPE_UINT},
  {"float", TK_TYPE_FLOAT},
  {"none",  TK_TYPE_NONE},
  {"#",     TK_TYPE_REF},
  {"@",     TK_TYPE_DEREF},
  {"->",    TK_TYPE_FN_ARROW},
  {"^",     TK_TYPE_TAG_UNION},
  {"&",     TK_TYPE_COMPOUND},
  {"::",    TK_TYPE_PARSE},

  //stmt blocks
  {"fn",    TK_BLOCK_FUNC},
  {"type",  TK_BLOCK_TYPE},
  {"if",    TK_BLOCK_IF},
  {"else",  TK_BLOCK_ELSE},
  {"while", TK_BLOCK_WHILE},
  {"loop",  TK_BLOCK_LOOP},
  {"for",   TK_BLOCK_FOR},
  {"match", TK_BLOCK_MATCH},
  {"load",  TK_BLOCK_LOAD},
  {"embed", TK_BLOCK_EMBED},
  {"skip",  TK_BLOCK_SKIP},
  {"stop",  TK_BLOCK_STOP},
  {"back",  TK_BLOCK_BACK},

  //assignment keywords
  {"=",     TK_ASSIGN},
  {"var",   TK_VARIABLE},
  {"const", TK_CONSTANT},
  {"+=",    TK_SUM_ASSIGN},
  {"-=",    TK_SUB_ASSIGN},
  {"*=",    TK_MUL_ASSIGN},
  {"/=",    TK_DIV_ASSIGN},
  {"%=",    TK_MOD_ASSIGN},
  {"++",    TK_INC_ASSIGN},
  {"--",    TK_DEC_ASSIGN},

  //symbols
  {"{",     TK_CUR_BRA_OPEN},
  {"}",     TK_CUR_BRA_CLOSE},
  {"[",     TK_SQR_BRA_OPEN},
  {"]",     TK_SQR_BRA_CLOSE},
  {"(",     TK_ROU_BRA_OPEN},
  {")",     TK_ROU_BRA_CLOSE},
  {"|",     TK_END_BAR},
  {":",     TK_COLON},
  {",",     TK_COMMA},
  {".",     TK_DOT},
  {"?",     TK_QUEST},
  {"!",     TK_EXCLA},
  {";",     TK_SEMICOLON},
  {";;",    TK_DOUB_SEMICOLON},
  {"=>",    TK_FN_RETURN},
  {"\n",    TK_NEW_LINE},
  {"\0",    TK_EOF},
};

char Lexer::nextChar() {
  if(fileIndex >= content.size()) return '\0';
  if(content[fileIndex-1] == '\n') {
    pos.l++;
    pos.e = 1;
  } else pos.e++;
  return content[fileIndex++];
}

char Lexer::peekChar() {
  if(fileIndex >= content.size()) return '\0';
  return content[fileIndex];
}

void Lexer::consumeWhiteSpaces() {
  if(this->curChar == ' ' || this->curChar == '\t')
    while((this->curChar = nextChar()) != '\0') {
      if(this->curChar == ' ' || this->curChar == '\t') continue;
      break;
    }
}

Token Lexer::nextWord() {
  consumeWhiteSpaces();
  Position pos = { this->pos.l, this->pos.e };
  while(this->specialChars.find(this->curChar) == string::npos && this->curChar != '\0') {
    this->curWord.push_back(this->curChar);
    this->curChar = nextChar();
  }
  if(this->curWord.empty()) {
    this->curWord.push_back(this->curChar);
    this->curChar = nextChar();
  }
  if(this->curWord.size() == 1) {
    if(this->curWord[0] == '$') { //comments
      this->curWord.clear();
      if(this->curChar == '$') { //block comments
        char prevChar = nextChar();
        this->curChar = nextChar();
        while(this->curChar != '\0') {
          prevChar = this->curChar;
          this->curChar = nextChar();
          if(prevChar == '$' && this->curChar == '$') break;
        }
        this->curChar = nextChar();
      } else {
        size_t pos = this->content.find('\n', this->fileIndex);
        this->fileIndex = pos == string::npos ? this->content.size() : pos;
      }
      return nextWord();
    } else if(this->curWord[0] == '\'' || this->curWord[0] == '\"' || this->curWord[0] == '`') {
      char endChar = this->curWord[0];
      while(this->curChar != endChar) {
        this->curWord.push_back(this->curChar);
        this->curChar = nextChar();
      }
      this->curWord.push_back(this->curChar);
      this->curChar = nextChar();
    } else if(this->doubleEspChars.find(this->curChar) != string::npos) {
      string doubleEsp = this->curWord + this->curChar;
      for( auto combination : this->possibleCombinations) {
        if(combination == doubleEsp) {
          this->curWord = doubleEsp;
          this->curChar = this->nextChar();
          break;
        }
      }
    }
  }
  Token tk = { 0, TK_NAME, pos, "" };
  if(regex_match(this->curWord, regex("[0-9_]+")))
    tk = { ++this->tokenId, TK_INT, pos, this->curWord };
  else if(regex_match(this->curWord, regex("[0-9_]*\\.[0-9_]*(e[-+]?[0-9_])?")))
    tk =  { ++this->tokenId, TK_FLOAT, pos, this->curWord };
  else if(regex_match(this->curWord, regex("([a-zA-Z](\\w)*\\.)+[a-zA-Z](\\w)*")))
    tk = { ++this->tokenId, TK_DOTTED_NAME, pos, this->curWord };
  else if(regex_match(this->curWord, regex("\"[^\"]*\"")))
    tk = { ++this->tokenId, TK_STR, pos, this->curWord };
  else if(regex_match(this->curWord, regex("\'[^\']*\'")))
    tk = { ++this->tokenId, TK_CHAR, pos, this->curWord };
  else if(regex_match(this->curWord, regex("`.*`")))
    tk = { ++this->tokenId, TK_INLINE_C, pos, this->curWord };

  for(size_t i = 0; i < LEN_BUILTIN_WORDS-1; i++) { //compare with every builtin word, except EOF
    if(!this->curWord.compare(builtinWords[i].symbol)) {
      tk = { ++this->tokenId, builtinWords[i].tokenType, pos, this->curWord };
    }
  }
  if(regex_match(this->curWord, regex("[A-Z][A-Za-z0-9_]*")))
    tk = { ++this->tokenId, TK_CAP_NAME, pos, this->curWord }; //capitalized words
  else if(regex_match(this->curWord, regex("([A-Za-z]+[0-9_]*)+")))
    tk = { ++this->tokenId, TK_NAME, pos, this->curWord }; //words
  if(!this->curWord.empty() && this->curWord[0] == '\0') tk = { ++this->tokenId, TK_EOF, pos, this->curWord };

  if(tk.id) {
    this->curWord.clear();
    return tk;
  }
  else if(tk.id == 0 && this->curChar == '\n') {
    printf("Next Word Error: Unrecognized token %s at line %lu and column %lu\n", this->curWord.c_str(), pos.l, pos.e);
    exit(1);
  }
  this->curWord.push_back(this->curChar);
  this->curChar = nextChar();
  return nextWord(); //try again with the next char, used on 1.e-2, as '-' is a special char
}

  const char *humanReadableType[MARKER+1] = {"Word", "Capitalized Word", "Integer Number", "String", "Char", "Floating Point", "Dotted Name", "C Code", "Builtin Word"};
void Lexer::printToken(Token token) {
  if(token.type == TK_NEW_LINE) printf("\tnew_line\n");
  else
    printf("[item: %s, type: %s, line: %lu, col: %lu, id: %lu]\n", token.text.c_str(), humanReadableType[token.type > MARKER ? MARKER : token.type], token.pos.l, token.pos.e, token.id);
}

#endif

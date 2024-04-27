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
  {"char",  TK_TYPE_CHAR},
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
  {"for",   TK_BLOCK_FOR},
  {"match", TK_BLOCK_MATCH},
  {"load",  TK_BLOCK_LOAD},
  {"embed", TK_BLOCK_EMBED},
  {"skip",  TK_BLOCK_SKIP},
  {"stop",  TK_BLOCK_STOP},
  {"back",  TK_BLOCK_BACK},

  //assignment keywords
  {"var",   TK_VARIABLE},
  {"const", TK_CONSTANT},
  {"=",     TK_ASSIGN},
  // TODO: add here assigns with operations: +=, -=, *=, /=, %=...

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

  //special tokens
  {"sys",   SYSCALL_TK},
  {"dump",  PRINT_INT}, // TODO: remove this
};

Token *createToken(string text, size_t id, TokenType type, size_t l, size_t c) {
    return new Token(text, id, type, l, c);
}

bool isValidChar(string word) {
  if(word.size() > 2 && word[0] == '\'' && word[word.size()-1] == '\'') {
    if(word[1] == '\\' && word.size() == 4) {
      string validChars = "rnt0\\\'\"";
      for ( char c : validChars) {
        if(word[2] == c) return true;
      }
    }
    if(word.size() == 3 && word[1] != '\\') 
      return true;
  }
  return false;
}

TokenType typeOfToken(string word) {
  int len = word.size();
  if(word[0] == '"') return TK_STR;
  if(word[0] == '`') return TK_INLINE_C;

  if(word[0] == '\'' && isValidChar(word)) return TK_CHAR;

  //number validation
  int i = 0;
  enum numberBase {DEC, HEX, OCT, BIN};
  enum numberBase base = DEC;
  if(word[i] == '0') {
    if(len == 1) return TK_INT;
    i+=2;
    if(word[i-1] == 'x') base = HEX;
    else if(word[i-1] == 'b') base = BIN;
    else if(word[i-1] == 'o') base = OCT;
    else i--;
  }

  switch (base) {
    case DEC:
      for(; i < len; i++)
        if(word[i] > 57 || word[i] < 48) break;
      break;
    case HEX:
      for(; i < len; i++) {
        if((word[i] < 58 && word[i] > 47) || (word[i] < 71 && word[i] > 64) || (word[i] < 103 && word[i] > 96)) continue;
        else break;
      }
      break;
    case OCT:
      for(; i < len; i++)
        if(word[i] > 55 || word[i] < 48) break;
      break;
    case BIN:
      for(; i < len; i++)
        if(word[i] != '0' && word[i] != '1') break;
      break;
  }
  if(len == i) return TK_INT;

  for(i = 0; i < LEN_BUILTIN_WORDS; i++) {
    if(!word.compare(builtinWords[i].symbol)) 
      return  builtinWords[i].tokenType;
  }

  return TK_NAME;
}

TokenizedLine *createTokenizedLine() {
  return new TokenizedLine();
}

TokenizedFile *createTokenizedFile() {
  return new TokenizedFile();
}

FileReader createFileReader(const char *file) {
  FILE *f = fopen(file, "r");
  if(!f) {
    fprintf(stderr, "Error! Could not open file %s\n", file);
    exit(1);
  }
  fclose(f);
  return {
    .file = ifstream(file),
    .content = string(),
    .word = string(),
    .currPos = 0,
    .currEndOfWord = 0,
    .currLine = 1,
    .currCol = 1,
  };
}

/*
 * This function can be used to save the curr state of the TokenizedFile
 * this way you can two or more cursors to walk over the Tokens
 * You must dealocate the copy too
*/
TokenizedFile *cloneTokenizedFile(const TokenizedFile tf) {
  TokenizedFile *clone = new TokenizedFile();
  clone->pos.l = tf.pos.l;
  clone->pos.e = tf.pos.e;
  clone->lines = tf.lines;
  return clone;
}

/*
 * This function is used to get the current Token
*/
Token *currToken(TokenizedFile tf) {
  if(tf.lines.size() <= tf.pos.l || tf.lines[tf.pos.l]->tokens.size() <= tf.pos.e)
    return NULL;
  return tf.lines[tf.pos.l]->tokens[tf.pos.e];
}

/*
 * This function is used to get the next Token of the file advancing TokenizedFile
 * Will return NULL at the end of all Tokens
*/
Token *nextToken(TokenizedFile *tf, size_t n) {
  if(currToken(*tf) == NULL) return NULL;
  for(size_t i = 0; i < n; i++)
    if(tf->lines[tf->pos.l]->tokens.size() <= ++tf->pos.e) {
      //if there are no more lines to iterate over or the line is empty, then return NULL
      if(tf->lines.size() == tf->pos.l+1 || tf->lines[tf->pos.l+1]->tokens.size() == 0) {
        return NULL;
      }

      tf->pos.e = 0;
      tf->pos.l++;
    }
  return currToken(*tf);
}
// same as above, but searching only in the current line
Token *nextLineToken(TokenizedFile *tf, size_t n) {
  size_t currLine = tf->pos.l;
  if(nextToken(tf, n) && tf->pos.l == currLine) 
    return currToken(*tf);
  else {
    returnToken(tf, n);
    return NULL;
  }
}

/*
 * This function is used to get the next Token of the file without advancing TokenizedFile
*/
Token *peekToken(TokenizedFile tf, size_t n) {
  TokenizedFile tmp = tf;
  return nextToken(&tmp, n);
}
// same as above, but searching only in the current line
Token *peekLineToken(TokenizedFile tf, size_t n) {
  TokenizedFile tmp = tf;
  return nextLineToken(&tmp, n);
}

/*
 * This function is used to get the previous Token of the file returning TokenizedFile
 * Will return NULL at the begin of all Tokens
*/
Token *returnToken(TokenizedFile *tf, size_t n) {
  for(size_t i = 0; i < n; i++) {
    if(!tf->pos.e) { //if it is the first element of the line
      if(!tf->pos.l) //if it is the first line
        return NULL;
      tf->pos.l--;
      tf->pos.e = tf->lines[tf->pos.l]->tokens.size();
    }
    tf->pos.e--;
  }
  return currToken(*tf);
}
// the same as above, but searching only in the current line
Token *returnLineToken(TokenizedFile *tf, size_t n) {
  size_t currLine = tf->pos.l;
  if(returnToken(tf, n) && tf->pos.l == currLine)
    return currToken(*tf);
  else 
    return NULL;
}

/*
 * This function is used to get the previous Token of the file without returning TokenizedFile
 * Will return NULL at the begin of all Tokens
*/
Token *peekBackToken(TokenizedFile tf, size_t n) {
  TokenizedFile tmp = tf;
  return returnToken(&tmp, n);
}
// the same as above, but searching only in the current line
Token *peekBackLineToken(TokenizedFile tf, size_t n) {
  TokenizedFile tmp = tf;
  return returnLineToken(&tmp, n);
}

/*
 * Try to advance the line, 0 if cannot, 1 if can
 * If it cannot advance the line it will update the pos.e to the last element of the line
*/
int advanceLineTokenizdFile(TokenizedFile *tf) {
  if(tf->pos.l == tf->lines.size() - 1) {
    tf->pos.e = tf->lines[tf->pos.l]->tokens.size() - 1;
    return 0;
  }
  tf->pos.l++;
  tf->pos.e = 0;
  return 1;
}

size_t getLineIndentation(TokenizedLine *line) {
  if(line->tokens.size() == 0) return 0;
  return line->tokens[0]->c;
}

/*
 * Return the id of the last word of the block and it's line
*/
struct endOfBlock endOfCurrBlock(TokenizedFile tf) {
  size_t identationBlock = currToken(tf)->c;
  size_t firstLine = currToken(tf)->l;
  do {
    if(!advanceLineTokenizdFile(&tf)) break;
  } while(identationBlock < currToken(tf)->c);
  if(currToken(tf)->c <= identationBlock && firstLine != currToken(tf)->l)
    returnToken(&tf, 1);
  /* printf("last word: %s\n", currToken(tf)->text); */

  return { currToken(tf)->id, currToken(tf)->l };
}

void printTokenizedFile(TokenizedFile p) {
    const char *humanReadableType[MARKER+1] = {"Word", "Integer Number", "String", "Char", "Floating Point", "C Code", "Builtin Word"};
    for(size_t i = 0; i < p.lines.size(); i++) {
        for(size_t j = 0; j < p.lines[i]->tokens.size(); j++) {
            printf("[id: %d line: %d, col: %d, item: %s, type: %s]\n", (int)p.lines[i]->tokens[j]->id , (int)p.lines[i]->tokens[j]->l, (int)p.lines[i]->tokens[j]->c, p.lines[i]->tokens[j]->text.c_str(),
                  humanReadableType[p.lines[i]->tokens[j]->type > MARKER ? MARKER : p.lines[i]->tokens[j]->type]);
        }
        printf("\n");
    }
}

void readFile(FileReader *fr) {
  fr->content = string((std::istreambuf_iterator<char>(fr->file)),
                       std::istreambuf_iterator<char>());
}

void addWordAsToken(TokenizedFile *tf, FileReader *fr, size_t *numWord) {
    if(!fr->word.size()) return;
    (*numWord)++; //unique id for each word of the file

    TokenizedLine *lastLine = tf->lines[tf->lines.size() - 1];
    //if the last line is not empty and the line of the last token is different from the current line
    if(lastLine->tokens.size() != 0 && lastLine->tokens[0]->l != fr->currLine) {
      tf->lines.push_back(createTokenizedLine());
      lastLine = tf->lines[tf->lines.size() - 1];
    }
    if(fr->word == ";;") { //forced end of the line
      tf->lines.push_back(createTokenizedLine());
      fr->word.clear();
      return;
    }

    lastLine->tokens.push_back(
      createToken(
        fr->word, //WARNING: maybe a bug here, the word is not being copied
        *numWord,
        typeOfToken(fr->word),
        fr->currLine,
        fr->currCol
    ));
    fr->word.clear();
}

void advanceCurrPos(FileReader *fr) {
  if(fr->content.at(fr->currPos) == '\n') {
    fr->currLine++;
    fr->currCol = 1;
  }
  else fr->currCol++;
  fr->currPos++;
}

void advanceCurrPosTill(FileReader *fr, size_t pos) {
  while(fr->currPos < pos) {
    advanceCurrPos(fr);
  }
}

void setCurrPosWithNextValidChar(FileReader *fr) {
  string wordDelimiters = " \t\n";
  while(fr->currPos < fr->content.size() && wordDelimiters.find(fr->content.at(fr->currPos)) != string::npos) {
    advanceCurrPos(fr);
  }
}

void setEndOfCurrWordWithDelimiter(FileReader *fr) {
  string wordDelimiters = " \t\n";
  while(fr->currEndOfWord < fr->content.size() && wordDelimiters.find(fr->content.at(fr->currEndOfWord)) == string::npos) {
    fr->currEndOfWord++;
  }
}

int getNextWord(FileReader *fr) {
  if(fr->currPos < fr->currEndOfWord) {
    fr->word = fr->content.substr(fr->currPos, fr->currEndOfWord - fr->currPos);
    return 0;
  }
  setCurrPosWithNextValidChar(fr);
  fr->currEndOfWord = fr->currPos;
  setEndOfCurrWordWithDelimiter(fr);
  fr->word = fr->content.substr(fr->currPos, fr->currEndOfWord - fr->currPos);
  return fr->word == "";
}

void lineJoinBySemicolon(TokenizedFile *tf) {
  size_t len = tf->lines.size();
  for(size_t i = 0; i < len; i++) {
    TokenizedLine *line = tf->lines[i];
    Token *lastToken = line->tokens[line->tokens.size() - 1];
    if(lastToken->text == ";") {
      if(len - 1 == i) {
        printf("There is no line to join with ';', at line: %zu\n", lastToken->l);
        exit(1);
      }
      TokenizedLine *nextLine = tf->lines[i+1];
      line->tokens.pop_back(); //removin the ';'
      line->tokens.insert(line->tokens.end(), nextLine->tokens.begin(), nextLine->tokens.end());
      tf->lines.erase(tf->lines.begin() + i+1);
      len--; i--;
    }
  }
}

TokenizedFile *readToTokenizedFile(const char *file) {
  TokenizedFile *tf = createTokenizedFile();
  tf->lines.push_back(createTokenizedLine()); //add the first line

  FileReader fr = createFileReader(file);
  readFile(&fr);
  size_t numWord = 0, substrPos = 0, firstSpecialCharPos = numeric_limits<size_t>::max(); //comments = 0, EndOfTheWord = 0;

  // doubleEspChars can be concatenated with themselves, they follow the combinations bellow
  const string doubleEspChars = ";=-+*/%<!>:"; 
  const vector<string> possibleCombinations = {
    "==", "!=", ">=", "<=", "++", "--", "+=", "-=", "*=", "/=", "%=", "<<", ">>", "<>", "->", "<-", "=>", "::", ";;"
  };
  const string singleEspChars = "()[]{}@#&|?,.`\'\"";
  const string specialChars = doubleEspChars + singleEspChars + '$'; //'$' for comments

  #define ADD_WORD_TILL(pos) fr.word = fr.word.substr(0, pos); \
                                   addWordAsToken(tf, &fr, &numWord); \
                                   advanceCurrPosTill(&fr, fr.currPos + pos); \
                                   getNextWord(&fr);

  while(1) {
    if(getNextWord(&fr)) break;

    // Finding the first special char in the word
    firstSpecialCharPos = numeric_limits<size_t>::max();
    for( char c : specialChars) {
      if((substrPos = fr.word.find(c)) != string::npos) {
        firstSpecialCharPos = min(firstSpecialCharPos, substrPos);
      }
    }

    // If there is no special char in the word
    if(firstSpecialCharPos == numeric_limits<size_t>::max()) {
      addWordAsToken(tf, &fr, &numWord);
      advanceCurrPosTill(&fr, fr.currEndOfWord);
      continue;
    }

    if(firstSpecialCharPos != 0) {
      ADD_WORD_TILL(firstSpecialCharPos);
    }

    if(doubleEspChars.find(fr.word[0]) != string::npos) { //is a double special char
      if(fr.word.size() > 1 && count(possibleCombinations.begin(), possibleCombinations.end(), fr.word.substr(0, 2)) != 0) {
        fr.word = fr.word.substr(0, 2);
        addWordAsToken(tf, &fr, &numWord);
        advanceCurrPosTill(&fr, fr.currPos + 2);
      } else {
        fr.word = fr.word.substr(0, 1);
        addWordAsToken(tf, &fr, &numWord);
        advanceCurrPosTill(&fr, fr.currPos + 1);
      }
    } else if(fr.word[0] == '$') { //comments
       string toFind = (fr.word.size() > 1 && fr.word[1] == '$') ? "$$" : "\n";
       substrPos = fr.content.find(toFind, fr.currPos + toFind.size());
       if(substrPos == string::npos) {
         fprintf(stderr, "Error! Comment not closed, at line: %d\n", (int)fr.currLine);
         exit(1);
       }
       advanceCurrPosTill(&fr, substrPos + toFind.size());
    } else if(fr.word[0] == '\"' || fr.word[0] == '\'' || fr.word[0] == '`') {
      //finding the next " that is not after a '\'
      substrPos = fr.currPos + 1;
      while((substrPos = fr.content.find(fr.word[0], substrPos)) 
            && fr.content[substrPos - 1] == '\\'
            && fr.content[substrPos - 2] != '\\' // if has 2 '\' in sequence pass
      ) {
        substrPos++;
      }
      if(substrPos == string::npos || fr.content.find('\n', fr.currPos + 1) < substrPos) {
        if(fr.word[0] == '`' && substrPos == string::npos) {
          fprintf(stderr, "Error! Inline C code not closed, at line: %d\n", (int)fr.currLine);
          exit(1);
        }
        else if(fr.word[0] != '`') {
          fprintf(stderr, "Error! String not closed, at line: %d\n", (int)fr.currLine);
          exit(1);
        }
      }
      fr.word = fr.content.substr(fr.currPos, substrPos - fr.currPos + 1);
      addWordAsToken(tf, &fr, &numWord);
      advanceCurrPosTill(&fr, substrPos + 1);
    } else { //other special chars
      fr.word = fr.word.substr(0, 1);
      addWordAsToken(tf, &fr, &numWord);
      advanceCurrPos(&fr);
    }
  }
  lineJoinBySemicolon(tf); //join lines that ends with ';'

  return tf;
}

void destroyTokenizdFile(TokenizedFile *tf) {
  for( auto line : tf->lines) {
    for( auto token : line->tokens) {
      delete token;
    }
    delete line;
  }
  delete tf;
}

#endif

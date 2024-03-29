#ifndef TOK_IMPL_
#define TOK_IMPL_

#include "tokenizer.h"
#include "utils.h"
#include <stddef.h>
#include <string>

struct SymbPrecedence {
  const char *symbol;
  TokenType tokenType;
  TokenPrecedence precedence;
};

static const struct SymbPrecedence builtinWords[COUNT_OF_TK_TYPES - NUM_DIV] = { //NUM_DIV is the first builtin word
  {"/", NUM_DIV, BUILTIN_LOW_PREC},
  {"*", NUM_MUL, BUILTIN_LOW_PREC},
  {"%", NUM_MOD, BUILTIN_LOW_PREC}, //the first three builtin words are binary operators with precedence
  {"not", LOG_NOT,    BUILTIN_SINGLE_OPERAND}, //precedence 1 to unary operations
  {"bnot", BIT_NOT,   BUILTIN_SINGLE_OPERAND},
  {"var", VARIABLE,   BUILTIN_SINGLE_OPERAND},
  {"int", TYPE_INT,   BUILTIN_SINGLE_OPERAND},
  {"str", TYPE_STR,   BUILTIN_SINGLE_OPERAND},
  {"load", LOAD_TK, BUILTIN_SINGLE_OPERAND},
  /* float */
  /* char */
  {"skip", SKIP_TK,  BUILTIN_SINGLE_OPERAND},
  {"stop", STOP_TK,  BUILTIN_SINGLE_OPERAND},
  {"@", DEREF_TK,  BUILTIN_SINGLE_OPERAND},
  {"+", NUM_ADD,    BUILTIN_MEDIUM_PREC},
  {"-", NUM_SUB,    BUILTIN_MEDIUM_PREC},
  {"==", CMP_EQ,    BUILTIN_MEDIUM_PREC},
  {"!=", CMP_NE,    BUILTIN_MEDIUM_PREC},
  {">=", CMP_GE,    BUILTIN_MEDIUM_PREC},
  {"<=", CMP_LE,    BUILTIN_MEDIUM_PREC},
  {">", CMP_GT,     BUILTIN_MEDIUM_PREC},
  {"<", CMP_LT,     BUILTIN_MEDIUM_PREC},
  {"or", LOG_OR,    BUILTIN_MEDIUM_PREC},
  {"and", LOG_AND,  BUILTIN_MEDIUM_PREC},
  {"band", BIT_AND, BUILTIN_MEDIUM_PREC},
  {"bor", BIT_OR,   BUILTIN_MEDIUM_PREC},
  {"shl", SHIFT_L_TK,   BUILTIN_MEDIUM_PREC},
  {"shr", SHIFT_R_TK,   BUILTIN_MEDIUM_PREC},
  {"=", ASSIGN,     BUILTIN_HIGH_PREC},
  {"if", IF_TK,        BUILTIN_HIGH_PREC},
  {"else", ELSE_TK,    BUILTIN_HIGH_PREC},
  {"while", WHILE_TK,  BUILTIN_HIGH_PREC},
  {"for", FOR_TK,      BUILTIN_HIGH_PREC},
  {"sys", SYSCALL_TK,      BUILTIN_HIGH_PREC},
  {"back", BACK_TK,  BUILTIN_HIGH_PREC},
  {"dump", PRINT_INT, BUILTIN_HIGH_PREC},
  {"fn", FUNC,      BUILTIN_HIGH_PREC},
  {"(", PAR_OPEN,   SYMBOLS},
  {")", PAR_CLOSE,  SYMBOLS},
  {"|", END_BAR,  SYMBOLS},
  {":", COLON,  SYMBOLS},
  {",", COMMA,  SYMBOLS},
  {";", SEMICOLON,  SYMBOLS},
  {";;", DOUBLE_SEMICOLON,  SYMBOLS},
  /* {"struct", BUILTIN_HIGH_PREC}, */
};

Token createToken(string text, size_t id, TkInfo info, size_t l, size_t c) {
    /* printf("creating token: %s %d\n", text, (int)len); */
    return {
      .id = id,
      .l = l,
      .c = c,
      .text = string(text),
      .type = info.type,
      .precedence = info.precedence
    };
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

TkInfo typeOfToken(string word) {
  int len = word.size();
  //compile time known values, str or int, has negative predecence: -1
  if(word[0] == '"') return {STR_TK, COMPTIME_KNOWN};

  if(word[0] == '\'' && isValidChar(word)) return {CHAR_TK, COMPTIME_KNOWN};

  //number validation
  int i = 0;
  enum numberBase {DEC, HEX, OCT, BIN};
  enum numberBase base = DEC;
  if(word[i] == '0') {
    if(len == 1) return {INT_TK, COMPTIME_KNOWN};
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
  if(len == i) return {INT_TK, COMPTIME_KNOWN};

  for(i = 0; i < COUNT_OF_TK_TYPES - NUM_DIV; i++) {
    if(!word.compare(builtinWords[i].symbol)) 
      return { builtinWords[i].tokenType, builtinWords[i].precedence };
  }

  return {NAME_TK, USER_DEFINITIONS};
}

TokenizedLine createTokenizedLine() {
    return {
      .tokens = vector<Token>()
    };
}

TokenizedFile createTokenizedFile() {
    return {
      .currLine = 0,
      .currElem = 0,
      .lines = vector<TokenizedLine>()
    };
}

FileReader createFileReader(const char *file) {
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
 * They will share the same memmory alocated for Tokenize the file, you
 * must not free a clone if you already freed the original one, or the opposite
*/
TokenizedFile cloneTokenizedFile(const TokenizedFile tf) {
  return {
    .currLine = tf.currLine,
    .currElem = tf.currElem,
    .lines = tf.lines,
  };
}

/*
 * This function is used to get the current Token
*/
Token *currToken(TokenizedFile tf) {
  if(tf.lines.size() == tf.currLine || tf.lines[tf.lines.size()-1].tokens.size() == tf.currElem)
    return NULL;
  return tf.lines[tf.currLine].tokens.data() + tf.currElem;
}

/*
 * This function is used to get the next Token of the file advancing TokenizedFile
 * Will return NULL at the end of all Tokens
*/
Token *nextToken(TokenizedFile *tf) {
  if(tf->lines[tf->currLine].tokens.size() == ++tf->currElem) {
    //if there are no more lines to iterate over or the line is empty, then return NULL
    if(tf->lines.size() == tf->currLine+1 || tf->lines[tf->currLine+1].tokens.size() == 0) {
      tf->currElem--;
      return NULL;
    }

    tf->currElem = 0;
    tf->currLine++;
  }
  return currToken(*tf);
}

/*
 * This function is used to get the next Token of the file without advancing TokenizedFile
*/
Token *peekToken(TokenizedFile tf) {
  TokenizedFile tmp = tf;
  return nextToken(&tmp);
}

/*
 * This function is used to get the previous Token of the file returning TokenizedFile
 * Will return NULL at the begin of all Tokens
*/
Token *returnToken(TokenizedFile *tf) {
  if(!tf->currElem) { //if it is the first element of the line
    if(!tf->currLine) //if it is the first line
      return NULL;
    tf->currLine--;
    tf->currElem = tf->lines[tf->currLine].tokens.size();
  }
  tf->currElem--;
  return currToken(*tf);
}

/*
 * This function is used to get the previous Token of the file without returning TokenizedFile
 * Will return NULL at the begin of all Tokens
*/
Token *peekBackTokenizedFile(TokenizedFile tf) {
  TokenizedFile tmp = tf;
  return returnToken(&tmp);
}

/*
 * Try to advance the line, 0 if cannot, 1 if can
 * If it cannot advance the line it will update the currElem to the last element of the line
*/
int advanceLineTokenizdFile(TokenizedFile *tf) {
  if(tf->currLine == tf->lines.size() - 1) {
    tf->currElem = tf->lines[tf->currLine].tokens.size() - 1;
    return 0;
  }
  tf->currLine++;
  tf->currElem = 0;
  return 1;
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
    returnToken(&tf);
  /* printf("last word: %s\n", currToken(tf)->text); */

  return { currToken(tf)->id, currToken(tf)->l };
}

void printTokenizedFile(TokenizedFile p) {
    const char *humanReadableType[NUM_DIV] = {"Word", "Integer Number", "String", "Char", "Floating Point", "Builtin Word"};
    for(size_t i = 0; i < p.lines.size(); i++) {
        for(size_t j = 0; j < p.lines[i].tokens.size(); j++) {
            printf("[id: %d line: %d, col: %d, item: %s, type: %s and prec: %d]\n", (int)p.lines[i].tokens[j].id , (int)p.lines[i].tokens[j].l, (int)p.lines[i].tokens[j].c, p.lines[i].tokens[j].text.c_str(),
                  humanReadableType[p.lines[i].tokens[j].type >= NUM_DIV ? (NUM_DIV - 1) : p.lines[i].tokens[j].type],
                  p.lines[i].tokens[j].precedence);
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

    TokenizedLine *lastLine = &tf->lines[tf->lines.size() - 1];
    //if the last line is not empty and the line of the last token is different from the current line
    if(lastLine->tokens.size() != 0 && lastLine->tokens[0].l != fr->currLine) {
      tf->lines.push_back(createTokenizedLine());
      lastLine = &tf->lines[tf->lines.size() - 1];
    }

    lastLine->tokens.push_back(
      createToken(
        fr->word, //WARNING: maybe a bug here, the word is not being copied
        *numWord,
        typeOfToken(fr->word), //len-1 to not count the '\0'
        fr->currLine,
        fr->currCol
    ));
    fr->word = string(); //clear the word
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

TokenizedFile readToTokenizedFile(const char *file) {
  TokenizedFile tf = createTokenizedFile();
  tf.lines.push_back(createTokenizedLine()); //add the first line

  FileReader fr = createFileReader(file);
  readFile(&fr);
  size_t numWord = 0, substrPos = 0, firstSpecialCharPos = numeric_limits<size_t>::max(); //comments = 0, EndOfTheWord = 0;

  // chars that can be concatenated with themselves
  // everyone besides the ';' can be concatenated with '=', and the ';' can be concatenated with itself
  const string doubleEspChars = ";=-+*/%<!>"; 
  const string singleEspChars = "()[]{}@#|,:.\'\"";
  const string specialChars = doubleEspChars + singleEspChars + '$'; //'$' for comments

  #define ADD_WORD_TILL(pos) fr.word = fr.word.substr(0, pos); \
                                   addWordAsToken(&tf, &fr, &numWord); \
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
      addWordAsToken(&tf, &fr, &numWord);
      advanceCurrPosTill(&fr, fr.currEndOfWord);
      continue;
    }

    if(firstSpecialCharPos != 0) {
      ADD_WORD_TILL(firstSpecialCharPos);
    }

    if(doubleEspChars.find(fr.word[0]) != string::npos) { //is a double special char
      if(fr.word.size() > 1 && ((fr.word[0] == ';' && fr.word[1] == ';') || (fr.word[0] != ';' && fr.word[1] == '='))) {
        fr.word = fr.word.substr(0, 2);
        addWordAsToken(&tf, &fr, &numWord);
        advanceCurrPosTill(&fr, fr.currPos + 2);
      } else {
        fr.word = fr.word.substr(0, 1);
        addWordAsToken(&tf, &fr, &numWord);
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
    } else if(fr.word[0] == '\"' || fr.word[0] == '\'') {
      //finding the next " that is not after a '\'
      substrPos = fr.currPos + 1;
      while((substrPos = fr.content.find(fr.word[0], substrPos)) 
            && fr.content[substrPos - 1] == '\\'
            && fr.content[substrPos - 2] != '\\' // if has 2 '\' in sequence pass
      ) {
        substrPos++;
      }
      if(substrPos == string::npos || fr.content.find('\n', fr.currPos + 1) < substrPos) {
        fprintf(stderr, "Error! String not closed, at line: %d\n", (int)fr.currLine);
        exit(1);
      }
      fr.word = fr.content.substr(fr.currPos, substrPos - fr.currPos + 1);
      addWordAsToken(&tf, &fr, &numWord);
      advanceCurrPosTill(&fr, substrPos + 1);
    } else { //other special chars
      fr.word = fr.word.substr(0, 1);
      addWordAsToken(&tf, &fr, &numWord);
      advanceCurrPos(&fr);
    }
  }

  return tf;
}

void destroyTokenizdFile(TokenizedFile *tf) {
  // for(size_t i = 0; i < tf->qtdLines; i ++) {
  //   for(size_t j = 0; j < tf->lines[i].qtdElements; j++) {
  //     free(tf->lines[i].tk[j].text);
  //     tf->lines[i].tk[j].text = NULL;
  //   }
  //   free(tf->lines[i].tk);
  //   tf->lines[i].tk = NULL;
  // }
  // free(tf->lines);
  // tf->lines = NULL;
  (void) tf;
}

#endif

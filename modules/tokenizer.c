#ifndef TOK_IMPL_
#define TOK_IMPL_

#include "tokenizer.h"
#include <stddef.h>

struct SymbPrecedence {
  const char *symbol;
  int tokenType, precedence;
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

Token createToken(char *text, size_t len, size_t id, TkInfo info, int l, int c) {
    /* printf("creating token: %s %d\n", text, (int)len); */
    Token tmp = {
      .id = id,
      .qtdChars = len,
      .text = (char *) malloc(len * sizeof(char)),
      .l = l,
      .c = c,
      .info = info,
    };
    memcpy(tmp.text, text, len);
    return tmp;
}

TkInfo typeOfToken(const char *const word, int len) {
  //compile time known values, str or int, has negative predecence: -1
  if(word[0] == '"') return (TkInfo) {STR_TK, COMPTIME_KNOWN};

  if(word[0] == '\'' && len > 2) return (TkInfo) {CHAR_TK, COMPTIME_KNOWN};

  //number validation
  int i = 0;
  enum numberBase {DEC, HEX, OCT, BIN};
  enum numberBase base = DEC;
  if(word[i] == '0') {
    if(len == 1) return (TkInfo) {INT_TK, COMPTIME_KNOWN};
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
  if(len == i) return (TkInfo) {INT_TK, COMPTIME_KNOWN};

  for(i = 0; i < COUNT_OF_TK_TYPES - NUM_DIV; i++) {
    if(cmpStr(word, builtinWords[i].symbol))
      return (TkInfo){ builtinWords[i].tokenType, builtinWords[i].precedence };
  }

  return (TkInfo) {NAME_TK, USER_DEFINITIONS};
}

TokenizedLine createTokenizedLine() {
    return (TokenizedLine) {
      .qtdElements = 0,
      .capElements = 5,
      .tk = (Token *) malloc(sizeof(Token) * 5)
    };
}

TokenizedFile createTokenizedFile() {
    return (TokenizedFile) {
      .qtdLines = 0,
      .capLines = 5,
      .currLine = 0,
      .currElem = 0,
      .lines = (TokenizedLine *) malloc(sizeof(TokenizedLine) * 5)
    };
}

FileReader createFileReader(FILE *fd) {
  if(fd == NULL) {
    fprintf(stderr, "Error when creating FileReader! File does not exists\n");
    exit(1);
  }
  return (FileReader) {
    .fd = fd,
    .word = (char *) malloc(sizeof(char) * 100),
    .wordSize = 0,
    .wordCap = 100,
    .l = 1,
    .c = 0,
  };
}

/*
 * This function can be used to save the curr state of the TokenizedFile
 * this way you can two or more cursors to walk over the Tokens
 * They will share the same memmory alocated for Tokenize the file, you
 * must not free a clone if you already freed the original one, or the opposite
*/
TokenizedFile cloneTokenizedFile(const TokenizedFile tf) {
  return (TokenizedFile) {
    .qtdLines = tf.qtdLines,
    .capLines = tf.capLines,
    .currLine = tf.currLine,
    .currElem = tf.currElem,
    .lines = tf.lines,
  };
}

/*
 * This function is used to get the current Token
*/
Token *currToken(TokenizedFile tf) {
  if(tf.qtdLines == tf.currLine || tf.lines[tf.currLine].qtdElements == tf.currElem)
    return NULL;
  return tf.lines[tf.currLine].tk + tf.currElem;
}

/*
 * This function is used to get the next Token of the file advancing TokenizedFile
 * Will return NULL at the end of all Tokens
*/
Token *nextToken(TokenizedFile *tf) {
  if(tf->lines[tf->currLine].qtdElements == ++tf->currElem) {
    //if there are no more lines to iterate over or the line is empty, then return NULL
    if(tf->qtdLines == tf->currLine+1 || tf->lines[tf->currLine+1].qtdElements == 0) {
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
  if(!tf->currElem) {
    if(!tf->currLine)
      return NULL;
    tf->currLine--;
    tf->currElem = tf->lines[tf->currLine].qtdElements;
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
  if(tf->currLine == tf->qtdLines - 1) {
    tf->currElem = tf->lines[tf->currLine].qtdElements - 1;
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
  int identationBlock = currToken(tf)->c;
  int firstLine = currToken(tf)->l;
  do {
    if(!advanceLineTokenizdFile(&tf)) break;
  } while(identationBlock < currToken(tf)->c);
  if(currToken(tf)->c <= identationBlock && firstLine != currToken(tf)->l)
    returnToken(&tf);
  /* printf("last word: %s\n", currToken(tf)->text); */

  return (struct endOfBlock){ currToken(tf)->id, currToken(tf)->l };
}

void printTokenizedFile(TokenizedFile p) {
    const char *humanReadableType[NUM_DIV] = {"Word", "Integer Number", "String", "Char", "Floating Point", "Builtin Word"};
    for(size_t i = 0; i < p.qtdLines; i++) {
        for(size_t j = 0; j < p.lines[i].qtdElements; j++) {
            printf("[id: %d line: %d, col: %d, item: %s, type: %s and prec: %d]\n", (int)p.lines[i].tk[j].id , p.lines[i].tk[j].l, p.lines[i].tk[j].c, p.lines[i].tk[j].text,
                  humanReadableType[p.lines[i].tk[j].info.type >= NUM_DIV ? 5 : p.lines[i].tk[j].info.type],
                  p.lines[i].tk[j].info.precedence);
        }
        printf("\n");
    }
}

int takeWord(FileReader *fr, char *to_cpy);

void appendTokenizedLine(TokenizedFile *tf) {
  TokenizedLine *lastLine = tf->lines + tf->qtdLines - 1;
  if(lastLine->qtdElements == 0) return;

  maybeRealloc((void **)&(tf->lines), (int *)&(tf->capLines), tf->qtdLines, sizeof(TokenizedLine));
  tf->lines[tf->qtdLines++] = createTokenizedLine();
}

void addWordAsToken(TokenizedFile *tf, FileReader *fr, int *numWord) {
    if(!fr->wordSize) return;
    (*numWord)++; //unique id for each word of the file

    TokenizedLine *lastLine = tf->lines + tf->qtdLines - 1;
    //maybe realloc current line to append the new token
    maybeRealloc((void **)&(lastLine->tk), (int *)&(lastLine->capElements), lastLine->qtdElements, sizeof(Token));

    int len = fr->wordSize+1;
    char tmp[len];
    if(takeWord(fr, tmp) == 0) //when the word is taken the wordSize is set to 0
      return;

    lastLine->tk[lastLine->qtdElements++] = createToken(tmp,
                                                        len,
                                                        *numWord,
                                                        typeOfToken(tmp, len-1), //len-1 to not count the '\0'
                                                        fr->currLine,
                                                        fr->currCol);
}

int notEOF(FileReader *fr) { return fr->currChar != EOF; }

void putcharFileReader(FileReader *fr, char c) {
  if(!fr->wordSize) {
    fr->currLine = fr->l;
    fr->currCol = fr->c;
  }
  fr->wordSize++;
  maybeRealloc((void **) &fr->word, (int *)&fr->wordCap, fr->wordSize, sizeof(char));
  fr->word[fr->wordSize-1] = c;
}

char readChar(FileReader *fr) {
  if(notEOF(fr)) {
    fr->currChar = getc(fr->fd);
    if(fr->currChar == '\n') {
      fr->l++;
      fr->c = -1;
    }
    fr->c++;
  }
  return fr->currChar;
}

/*
 * Buffers used to copy the word value must have at least lenWord(fr) + 1 bytes
 */
int takeWord(FileReader *fr, char *to_cpy) {
  if(fr->wordSize) {
    memcpy(to_cpy, fr->word, fr->wordSize);
    to_cpy[fr->wordSize] = 0;
    fr->wordSize = 0;
    memset(fr->word, 0, fr->wordCap);
    return 1;
  }
  else return 0;
}

TokenizedFile readToTokenizedFile(FILE *fd) {
  TokenizedFile tf = createTokenizedFile();
  tf.lines[0] = createTokenizedLine();
  tf.qtdLines++;

  FileReader fr = createFileReader(fd);
  int numWord = 0, comments = 0;

  while(notEOF(&fr)) {
    readChar(&fr);
    if(comments) {
      if(comments == 1 && fr.currChar == '\n') comments = 0;
      else if(comments == 2 && fr.currChar == '$') {
        readChar(&fr);
        if(fr.currChar == '$') comments = 0;
      }
      //end of the comment
      if(!comments) {
        TokenizedLine *lastLine = tf.lines + tf.qtdLines - 1;
        if(lastLine->qtdElements) {
          if(lastLine->tk[0].l != fr.l) //add a new TokenizedLine if needed
            appendTokenizedLine(&tf);
          //print the lines
        }
      }
    }
    else {
      switch_begin:
      switch(fr.currChar) {
        case '\n': //between words
          addWordAsToken(&tf, &fr, &numWord);
          appendTokenizedLine(&tf);
          /* printf("newline\n"); */
          break;
        case '\t':
        case ' ':
          addWordAsToken(&tf, &fr, &numWord);
          /* printf("space\n"); */
          break;
        case '$': //comments
          //comment the rest of the line if read only a '$'
          //comment a block if read double '$', till the next double '$'
          addWordAsToken(&tf, &fr, &numWord);
          readChar(&fr);
          comments = (fr.currChar == '$') ? 2 : 1; //2 for block comments and 1 for line comments
          break;
        case '"': //strings
          putcharFileReader(&fr, fr.currChar);
          readChar(&fr);
          while(fr.currChar != '"') {
            if(fr.currChar == '\n') {
              fprintf(stderr, "Error! String not closed, at line: %d\n", fr.currLine);
              exit(1);
            }
            putcharFileReader(&fr, fr.currChar);
            readChar(&fr);
          }
          putcharFileReader(&fr, fr.currChar);
          break;
        case '\'': //chars
          putcharFileReader(&fr, fr.currChar);
          char c = readChar(&fr);
          if(readChar(&fr) == '\'' || c == '\\') {
            if(c == '\\') {
              putcharFileReader(&fr, c);
              c = fr.currChar;
              readChar(&fr);
            }
            putcharFileReader(&fr, c);
            putcharFileReader(&fr, fr.currChar);
          } else {
            fprintf(stderr, "Error! Bad format for char, at: %d %d\n", fr.currLine, fr.currCol);
            exit(1);
          }
          break;
        case ';':
          addWordAsToken(&tf, &fr, &numWord);
          putcharFileReader(&fr, fr.currChar);
          readChar(&fr);
          if(fr.currChar == ';') {
            putcharFileReader(&fr, fr.currChar);
            addWordAsToken(&tf, &fr, &numWord);
          }
          else {
            addWordAsToken(&tf, &fr, &numWord);
            goto switch_begin; //if it is not a ';' then we need to search for the case of the char read
          }
          break;
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
        case '@':
        case '#':
        case ':':
        case '|':
        case ',':
        case '.':
        case '?':
          addWordAsToken(&tf, &fr, &numWord);
          putcharFileReader(&fr, fr.currChar);
          addWordAsToken(&tf, &fr, &numWord);
          break;
        case '=':
        case '>':
        case '<':
        case '!':
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
          addWordAsToken(&tf, &fr, &numWord);
          putcharFileReader(&fr, fr.currChar);
          readChar(&fr);
          if(fr.currChar == '=') {
            putcharFileReader(&fr, fr.currChar);
            addWordAsToken(&tf, &fr, &numWord);
          }
          else {
            addWordAsToken(&tf, &fr, &numWord);
            goto switch_begin; //if it is not a '=' then we need to search for the case of the char read
          }
          /* printf("symbol: %c\n", fr.currChar); */
          break;
        default:
          putcharFileReader(&fr, fr.currChar);
          break;
      }
    }
  }
  TokenizedLine *lastLine = tf.lines + tf.qtdLines - 1;
  if(lastLine->qtdElements == 0) {
    tf.qtdLines--;
    free(lastLine->tk);
  }
  return tf;
}

void destroyTokenizdFile(TokenizedFile *tp) {
  for(size_t i = 0; i < tp->qtdLines; i ++) {
    for(size_t j = 0; j < tp->lines[i].qtdElements; j++) {
      free(tp->lines[i].tk[j].text);
      tp->lines[i].tk[j].text = NULL;
    }
    free(tp->lines[i].tk);
    tp->lines[i].tk = NULL;
  }
  free(tp->lines);
  tp->lines = NULL;
}

#endif

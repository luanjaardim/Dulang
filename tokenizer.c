#ifndef TOK_IMPL_
#define TOK_IMPL_

#include "tokenizer.h"
#include <stddef.h>

struct SymbPrecedence {
  const char *symbol;
  int tokenType, precedence;
};

struct SymbPrecedence builtinWords[COUNT_OF_TK_TYPES - NUM_DIV] = { //NUM_DIV is the first builtin word
  {"/", NUM_DIV, 0},
  {"*", NUM_MUL, 0},
  {"%", NUM_MOD, 0}, //the first three builtin words are binary operators with precedence
  /* {"var", 1}, */
  {"not", LOG_NOT, 1}, //precedence 1 to unary operations
  {"==", CMP_EQ, 2},
  {"!=", CMP_DIF, 2},
  {"+", NUM_ADD, 2},
  {"-", NUM_SUB, 2},
  {"or", LOG_OR, 2},
  {"and", LOG_AND, 2},
  {">=", CMP_GE, 2},
  {"<=", CMP_LE, 2},
  {">", CMP_GT, 2},
  {"<", CMP_LT, 2},
  {"band", BIT_AND, 2},
  {"bor", BIT_OR, 2},
  {"bnot", BIT_NOT, 2},
  {"=", ASSIGN, HIGH_PRECEDENCE - 1},
  {"fn", FUNC, HIGH_PRECEDENCE - 1},
  {"if", IF, HIGH_PRECEDENCE - 1},
  {"else", ELSE, HIGH_PRECEDENCE - 1},
  {"while", WHILE, HIGH_PRECEDENCE - 1},
  {"for", FOR, HIGH_PRECEDENCE - 1},
  {"(", PAR_OPEN, HIGH_PRECEDENCE - 1}, //this precedence will maybe change
  {")", PAR_CLOSE, HIGH_PRECEDENCE - 1}, //this precedence will maybe change
  /* {"struct", HIGH_PRECEDENCE - 1}, */
};

Token createToken(char *text, size_t len, size_t id, TkTypeAndPrecedence typeAndPrec, int l, int c) {
    Token tmp = {
      .id = id,
      .qtdChars = len,
      .text = (char *) malloc(sizeof(char) * len),
      .l = l,
      .c = c,
      .typeAndPrecedence = typeAndPrec,
    };
    memcpy(tmp.text, text, len);
    return tmp;
}

TkTypeAndPrecedence typeOfToken(const char *const word, int len) {
  //compile time known values, str or int, has negative predecence: -1

  //validation of str

  int i;
  for(i = 0; i < len; i++) //number validation
    if(word[i] > 57 || word[i] < 48) break;
  if(len == i) return (TkTypeAndPrecedence) {INT_TK, -1};

  for(i = 0; i < COUNT_OF_TK_TYPES - NUM_DIV; i++) {
    if(cmpStr(word, builtinWords[i].symbol))
      return (TkTypeAndPrecedence){ builtinWords[i].tokenType, builtinWords[i].precedence };
  }

  return (TkTypeAndPrecedence) {VAR_NAME_TK, HIGH_PRECEDENCE};
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
  return tf.lines[tf.currLine].tk + tf.currElem;
}

/*
 * This function is used to get a mutable reference to the current Token
*/
Token *currMutToken(TokenizedFile tf) {
  return tf.lines[tf.currLine].tk + tf.currElem;
}

/*
 * This function is used to get the next Token of the file advancing TokenizedFile
 * Will return NULL at the end of all Tokens
*/
Token *nextToken(TokenizedFile *tf) {
  if(tf->lines[tf->currLine].qtdElements == ++tf->currElem) {
    tf->currElem = 0;
    tf->currLine++;
    //if there are no more lines to iterate over or the line is empty, then return NULL
    if(tf->qtdLines == tf->currLine || tf->lines[tf->currLine].qtdElements == 0)
      return NULL;
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
 * Return the id of the last word of the block
*/
struct endOfBlock endOfCurrBlock(TokenizedFile tf) {
  int identationBlock = currToken(tf)->c;
  int firstLine = currToken(tf)->l;
  do {
    if(!advanceLineTokenizdFile(&tf)) break;
  } while(identationBlock < currToken(tf)->c);
  if(currToken(tf)->c <= identationBlock && firstLine != currToken(tf)->l)
    returnToken(&tf);

  return (struct endOfBlock){ currToken(tf)->id, currToken(tf)->l };
}

void printTokenizedFile(TokenizedFile p) {
    const char *humanReadableType[4] = {"Word", "Integer Number", "String", "Builtin Word"};
    for(size_t i = 0; i < p.qtdLines; i++) {
        for(size_t j = 0; j < p.lines[i].qtdElements; j++) {
            printf("[id: %d line: %d, col: %d, item: %s, type: %s and prec: %d]\n", (int)p.lines[i].tk[j].id , p.lines[i].tk[j].l, p.lines[i].tk[j].c, p.lines[i].tk[j].text,
                  humanReadableType[0],
                  p.lines[i].tk[j].typeAndPrecedence.precedence);
//min(p.lines[i].tk[j].typeAndPrecedence.type, NUM_DIV)
        }
        printf("\n");
    }
}

void cleanWord(char *word, int *sizeWord) {
    word[*sizeWord] = 0;
    memset(word, 0, *sizeWord);
    *sizeWord = 0;
}

void addWordAsToken(TokenizedLine *lastLine, char *word, int sizeWord, int *numWord, int fileLine, int fileCol) {
    (*numWord)++; //unique id for each word of the file

    //maybe realloc current line to append the new token
    maybeRealloc((void **)&(lastLine->tk), (int *)&(lastLine->capElements), lastLine->qtdElements, sizeof(Token));
    lastLine->tk[lastLine->qtdElements++] = createToken(word,
                                                        sizeWord,
                                                        *numWord,
                                                        typeOfToken(word, sizeWord),
                                                        fileLine,
                                                        fileCol-sizeWord);
}

TokenizedFile readToTokenizedFile(FILE *fd) {
    int fileLine = 0, fileCol = 0, numWord = 0; //file informations
    int sizeWord = 0, capWord = 10; //word informations
    char *word = (char *) calloc(10, sizeof(char));

    TokenizedFile p = createTokenizedFile();
    p.lines[p.qtdLines++] = createTokenizedLine();
    TokenizedLine *lastLine = p.lines + 0;

    //used for commenting/discarting everything when sees '$' or '$('
    u_int8_t toComment = 0;
    u_int8_t blockToComment = 0;

    char c = getc(fd);
    while(c != EOF) {

        if(c == ' ' || c == '\n') {
            if(!sizeWord) { //len of the current word is 0
                if(c == '\n') {
                    fileCol = -1;
                    fileLine++;

                    if(lastLine->qtdElements) {//if the line has elements create a new line
                      maybeRealloc((void **)&(p.lines), (int *)&(p.capLines), p.qtdLines, sizeof(TokenizedLine));
                      p.lines[p.qtdLines] = createTokenizedLine();

                      lastLine = p.lines + p.qtdLines;
                      p.qtdLines++;
                    }
                }
                goto end; //discarting empty words
            }

            addWordAsToken(lastLine, word, sizeWord, &numWord, fileLine, fileCol);

            if(c == '\n') {
                fileCol = -1;
                fileLine++;
                maybeRealloc((void **)&(p.lines), (int *)&(p.capLines), p.qtdLines, sizeof(TokenizedLine));
                p.lines[p.qtdLines] = createTokenizedLine();

                lastLine = p.lines + p.qtdLines;
                p.qtdLines++;
                //if it's a only line comment, comments are disabled
                toComment = blockToComment ? 1 : 0;
            }

            cleanWord(word, &sizeWord);
        }
        /* else if(c == '"') { */ //strings

        /* } */
        else if(c == '$') { //comments
            toComment = 1;
            c = getc(fd); //to comment blocks the next character must be a '('
            fileCol++;
            if( c == '(' ) blockToComment = 1;
        }
        else if(c == '(' || c == ')') {
            if(blockToComment && c == ')') { //exiting comments blocks
                toComment = 0;
                blockToComment = 0;
            }
            else {
              if(sizeWord)
                addWordAsToken(lastLine, word, sizeWord, &numWord, fileLine, fileCol);

              //add a size one word with just '(' or ')'
              sizeWord = 1;
              maybeRealloc((void **)&word, &capWord, sizeWord, sizeof(char));
              word[0] = c;
              addWordAsToken(lastLine, word, sizeWord, &numWord, fileLine, fileCol);
              cleanWord(word, &sizeWord);
            }
        }
        else if(!toComment){
            maybeRealloc((void **)&word, &capWord, sizeWord, sizeof(char));
            word[sizeWord++] = c;
        }

        end:
        fileCol++;
        c = getc(fd);
    }

    free(word);
    if(!p.lines[p.qtdLines-1].qtdElements) //empty last line
      free(p.lines[--p.qtdLines].tk);

    return p;
}

void destroyTokenizdFile(TokenizedFile tp) {
  for(size_t i = 0; i < tp.qtdLines; i ++) {
    for(size_t j = 0; j < tp.lines[i].qtdElements; j++) {
      free(tp.lines[i].tk[j].text);
      tp.lines[i].tk[j].text = NULL;
    }
    free(tp.lines[i].tk);
    tp.lines[i].tk = NULL;
  }
  free(tp.lines);
  tp.lines = NULL;
}

void maybeRealloc(void **pnt, int *const cap, int newSize, size_t elementSize) {
    if(*cap <= newSize) {
        (*cap) *= 2;
        *pnt = realloc(*pnt, (*cap) * elementSize);
        if(*pnt == NULL) {
            fprintf(stderr, "Fail to realloc\n");
            exit(1);
        }
    }
}
size_t lenStr(const char *const str) {
  size_t i = 0;
  while(str[i++] != 0);
  return i-1;
}
int cmpStr(const char *const str1, const char *const str2) {
  size_t i = 0;
  while(str1[i] == str2[i]) {
    if(str1[i++] == 0)
      return 1; //true
  }
  return 0; //false
}

#endif

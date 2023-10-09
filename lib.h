#ifndef LIB_H_
#define LIB_H_

#include <cstdlib>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

typedef struct Token Token;
typedef struct TokenizedLine TokenizedLine;
typedef struct TokenizedFile TokenizedFile;

TokenizedFile readFile(FILE *fd);
void printTokenizedFile(TokenizedFile p);

#endif // LIB_H_

/*
 * Implementation of some useful functions for parsing the file
 */
#ifndef LIB_IMPL_H
#define LIB_IMPL_H

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

typedef struct Token {
    size_t qtdChars;
    char *text;
    int l, c; //line and column
} Token;

Token createToken(char *text, size_t len, int l, int c) {
    Token tmp = {
      .qtdChars = len,
      .text = (char *) malloc(sizeof(char) * len),
      .l = l,
      .c = c,
    };
    memcpy(tmp.text, text, len);
    return tmp;
}

typedef struct TokenizedLine {
    size_t qtdElements, capElements;
    Token *tk;
} TokenizedLine;

TokenizedLine createTokenizedLine() {
    return (TokenizedLine) {
      .qtdElements = 0,
      .capElements = 5,
      .tk = (Token *) malloc(sizeof(Token) * 5)
    };
}

typedef struct TokenizedFile {
    size_t qtdLines, capLines;
    TokenizedLine *lines;
} TokenizedFile;

TokenizedFile createTokenizedFile() {
    return (TokenizedFile) {
      .qtdLines = 0,
      .capLines = 5,
      .lines = (TokenizedLine *) malloc(sizeof(TokenizedLine) * 5)
    };
}

void printTokenizedFile(TokenizedFile p) {
    for(int i = 0; i < p.qtdLines; i++) {
        for(int j = 0; j < p.lines[i].qtdElements; j++) {
            printf("[line: %d, col: %d, item: %s ]\n", p.lines[i].tk[j].l, p.lines[i].tk[j].c, p.lines[i].tk[j].text);
        }
        printf("\n");
    }
}

TokenizedFile readFile(FILE *fd) {
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
                }
                goto end; //discarting empty words
            }
            numWord++; //unique id for each word of the file
            //maybe realloc current line to append the new token
            maybeRealloc((void **)&(lastLine->tk), (int *)&(lastLine->capElements), lastLine->qtdElements, sizeof(Token));
            lastLine->tk[lastLine->qtdElements++] = createToken(word, sizeWord, fileLine, fileCol-sizeWord);

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

            word[sizeWord] = 0;
            memset(word, 0, sizeWord);
            sizeWord = 0;
        }
        else if(c == '$') { //comments
            toComment = 1;
            printf("%c\n", c);
            c = getc(fd); //to comment blocks the next character must be a '('
            printf("%c\n", c);
            if( c == '(' ) blockToComment = 1;
            printf("%d %d\n", fileLine, blockToComment);
        }
        else if(c == '(' || c == ')') {
            printf("%d %d\n", fileLine, blockToComment);
            if(blockToComment && c == ')') { //exiting comments blocks
                toComment = 0;
                blockToComment = 0;
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
    return p;
}

#endif // LIB_IMPL_H

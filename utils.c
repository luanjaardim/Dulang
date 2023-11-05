#include "utils.h"

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

void swap(void *a, void *b, size_t size) {
  char *c = (char *)a;
  char *d = (char *)b;
  char tmp;
  for(size_t i = 0; i < size; i++) {
    tmp = c[i];
    c[i] = d[i];
    d[i] = tmp;
  }
}

void insertIntToStr(FILE *f) {
    FILE *toRead = fopen("intToStr.asm", "r");
    char c;
    while((c = fgetc(toRead)) != EOF) {
        fputc(c, f);
    }
    fclose(toRead);
}

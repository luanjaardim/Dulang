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
    char *intToStrFunc =
    "int_to_str: \
        ;; 64 bits to store the number\n \
        xor rcx, rcx\n \
        mov rcx, rsp\n \
        dec rcx\n \
        ;; 32 bytes to store the number\n \
        times 4 push qword 0\n \
        mov byte[rcx], 10\n \
        dec rcx\n \
        xor r8, r8\n \
        inc r8\n \
    \n \
        ;; number to convert\n \
        mov rax, [rsp + 40]\n \
        .loop:\n \
        ;; divide by 10\n \
        mov rbx, 10\n \
        xor rdx, rdx\n \
        div rbx\n \
        ;; push the remainder\n \
        add rdx, '0'\n \
        mov byte[rcx], dl\n \
        dec rcx\n \
        inc r8\n \
    \n \
        ;; if the number is not 0, loop\n \
        cmp rax, 0\n \
        jne .loop\n \
    \n \
        .end:\n \
        inc rcx\n \
    \n \
    ;;; print the number\n \
        mov rdi, 1\n \
        mov rsi, rcx\n \
        mov rdx, r8\n \
        mov rax, 1\n \
        syscall\n \
        add rsp, 32\n \
    \n \
        ret\n";
    fprintf(f, "%s", intToStrFunc);
}

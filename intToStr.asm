int_to_str:
    ;; 64 bits to store the number
    xor rcx, rcx
    mov rcx, rsp
    dec rcx
    ;; 32 bytes to store the number
    times 4 push qword 0
    mov byte[rcx], 10
    dec rcx
    xor r8, r8
    inc r8

    ;; number to convert
    mov rax, [rsp + 40]
    .loop:
    ;; divide by 10
    mov rbx, 10
    xor rdx, rdx
    div rbx
    ;; push the remainder
    add rdx, '0'
    mov byte[rcx], dl
    dec rcx
    inc r8

    ;; if the number is not 0, loop
    cmp rax, 0
    jne .loop

    .end:
    inc rcx

;;; print the number
    mov rdi, 1
    mov rsi, rcx
    mov rdx, r8
    mov rax, 1
    syscall
    add rsp, 32

    ret

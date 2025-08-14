;
; A simple bootloader that initializes segment registers and prints "Hello, World!"
;
[org 0x7c00]

;
; Initialize segment registers
;
mov ax, 0x0000
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x07c00
;
; Print "Hello, World!"
;
mov ah, 0x0e ; teletype function
mov bx, 0x0007 ; white text on black background
mov si, message
;
print:
    lodsb
    cmp al, 0
    je done
    int 0x10 ; call BIOS interrupt
    jmp print
;
done:
    jmp $ ; infinite loop
;
; Data
;
message db 'Hello, World!', 0
;
; Boot sector padding
;
times 510-($-$$) db 0
dw 0xaa55

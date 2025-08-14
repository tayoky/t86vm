bits 16
mov ax, 0x7
mov ds, ax
mov ss, ax
mov es, ax
mov fs, ax
mov ah, 0x89
mov bx, 0x4848
mov word[0x7c00], bx
mov cx, [0x7c00]
hlt

times 510 - ($ - $$) db 0x00
db 0x55
db 0xaa

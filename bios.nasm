; test bios
org 0xf0000
bits 16
times 0xfff0 - ($ - $$) db 0x00
_start:
mov ax, 0x4848
hlt
times 0x10000 - ($ - $$) db 0x00

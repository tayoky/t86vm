;test conditional jump

bits 16
xor ax, ax
inc ax
dec ax
jnz jump
mov bx, 0x00
hlt
jump:
mov bx, 0x4848
hlt

times 510 - ($-$$) db 0x00
db 0x55
db 0xaa

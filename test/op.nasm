;test various stuff
bits 16
mov dx, 0x6181
mov [0x6c00], dx
xor dx, [0x6c00]
mov bx, 8
mov cx, bx
add cx, bx
hlt

times 510 - ($ -$$) db 0x10
db 0x55
db 0xaa

;test push/pop
mov ax, 0x4848
mov sp, 0x7c00
push ax
pop bx
hlt

times 512 - ($ -  $$) db 0x00
db 0x55
db 0xaa

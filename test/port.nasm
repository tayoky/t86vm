bits 16
mov al, 'h'
out 0x00, al
hlt

times 510 - ($ - $$) db 0x00
db 0x55
db 0xaa

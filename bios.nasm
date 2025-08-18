; test bios
org 0xf0000
bits 16

start:
;setup stack
	xor dx, dx
	mov ss, dx
	mov sp, 0x8000
;register handler for interupt 0x10
	lea dx, test_handler
	mov [0x10 * 4],dx
	mov [0x10 * 4 + 2], cs
	sti
;test the interupt
	int 0x10
;test loop
	mov cx, 16
	xor ax, ax
l:
	out 0x80, ax
	inc ax
	loop l
;now test js
	mov bx, 0x8000
	test bx, bx
	js js_good
	xor bx, bx
js_good:
	hlt

test_handler:
	mov ax,0x4848
	iret

times 0xfff0 - ($ - $$) db 0x00

;reset vector
_start:
	jmp 0xf000:start

times 0x10000 - ($ - $$) db 0x00

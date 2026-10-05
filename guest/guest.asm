bits 16
org 0x1000


start:
    mov ax, 0x2
    add ax, 0x3

    add al, '0'
    mov dx, 0xE9
    out dx, al

    mov al, 10
    out dx, al

    hlt
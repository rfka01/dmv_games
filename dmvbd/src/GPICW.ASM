; GPICW.ASM - DMVBD: Aufruf des DMVGRAF-Kerns GPIC.INC (gepackte
; Vollbilder, Kennung DMVP, LZSS) aus Open-Watcom-C.
;
; int __cdecl gpic_datei(const char *name, int art, int xw, int y,
;                        char *arbeit)
;   arbeit: mindestens GP_GROESSE (5311) Byte in DGROUP
;   Rueck: 0 oder Fehlernummer von gp_picw (92 Datei fehlt, ...)
; JWasm -Zm. Code: Claude, Prompts: rfka01

        .8086
GP_NURKERN EQU 1

_TEXT   SEGMENT WORD PUBLIC 'CODE'
        ASSUME  CS:_TEXT

        INCLUDE GPIC.INC

        PUBLIC  _gpic_datei
_gpic_datei PROC NEAR
        push    bp
        mov     bp, sp
        push    si
        push    di
        push    es
        mov     dx, [bp+4]
        mov     al, [bp+6]
        mov     bx, [bp+8]
        mov     cx, [bp+10]
        mov     di, [bp+12]
        push    ds
        pop     es
        call    gp_picw
        jc      gd9
        xor     ax, ax
gd9:    pop     es
        pop     di
        pop     si
        pop     bp
        ret
_gpic_datei ENDP

_TEXT   ENDS
        END

; BLT86.ASM - DMVBD: eine Bitmapzeile an den uPD7220 der NCR DMV
; (innere Schleife des Bildschirmtreibers, 8086). JWasm.
;
; void __cdecl blit_row(WORD addr, WORD mode, WORD fm, WORD lm, WORD n,
;                       WORD qoff, WORD qseg, WORD b, WORD inv)
;   addr  Wortadresse des ersten Bildschirmworts (mit Ebene)
;   mode  WDAT-Modus 0..3
;   fm,lm Masken des ersten und letzten Worts (n = 1: fm schon verundet)
;   n     Zahl der Worte (1..40)
;   q     Quelle (Segment:Versatz). Bitmaps liegen im Format des 7220:
;         Bit 0 links, Bytes von links nach rechts. Wort k umfasst die
;         Bits b + 16k .. b + 16k + 15 ab q (b = 0..7). Bits links vom
;         Anfang gehoeren zur Vorzeile - die Randmaske blendet sie aus.
;   inv   0 oder FFFFh: Quelle invertieren
;
; Ablauf je Zeile: CURS, MASK fm, WDAT, Wort 0; dann MASK FFFFh, WDAT,
; Worte 1..n-2; dann MASK lm, WDAT, letztes Wort. FIGS (Richtung 2,
; DC 0) setzt der Aufrufer vorher einmal.
; Fuer die Mittelworte gibt es je Verschiebung b eine eigene Schleife
; (b-mal SHR DX,1 / RCR AX,1 ausgeschrieben), damit kein SHL/SHR mit CL
; (auf dem 8088 4 Takte je Stelle plus Grundzeit) und kein Aufruf noetig ist.
;
; Ausgabe in Buendeln: der FIFO des 7220 fasst 16 Bytes. An festen
; Stellen (Zeilenanfang, alle 8 Mittelworte, vor dem letzten Wort) wird
; gewartet, bis er leer ist (Status Bit 2); dazwischen geht jedes Byte
; ohne Abfrage hinaus, nie mehr als 16 am Stueck.
; Code: Claude, Prompts: rfka01

        .8086
_TEXT   SEGMENT WORD PUBLIC 'CODE'
        ASSUME  CS:_TEXT

A_ADDR  EQU     4
A_MODE  EQU     6
A_FM    EQU     8
A_LM    EQU     10
A_N     EQU     12
A_QO    EQU     14
A_QS    EQU     16
A_B     EQU     18
A_INV   EQU     20

WLEER   MACRO
        LOCAL   w
w:      in      al, 0A0h
        test    al, 4
        jz      w
        ENDM

; Wort aus DS:SI (b = CL Stellen verschoben) nach AX, SI += 2; BX = inv
HOLEN   MACRO
        LOCAL   s1, s9
        mov     ax, [si]
        mov     dx, [si+2]
        xor     ax, bx
        xor     dx, bx
        or      cl, cl
        jz      s9
        push    cx
s1:     shr     dx, 1
        rcr     ax, 1
        dec     cl
        jnz     s1
        pop     cx
s9:     add     si, 2
        ENDM

        PUBLIC  _blit_row
_blit_row PROC  NEAR
        push    bp
        mov     bp, sp
        push    si
        push    di
        push    ds
        mov     si, [bp+A_QO]
        mov     ds, [bp+A_QS]
        mov     bx, [bp+A_INV]
        mov     cl, [bp+A_B]
        WLEER
        mov     al, 49h                 ; CURS addr, Punktadresse 0   4
        out     0A1h, al
        mov     ax, [bp+A_ADDR]
        out     0A0h, al
        mov     al, ah
        out     0A0h, al
        xor     al, al
        out     0A0h, al
        mov     al, 4Ah                 ; MASK fm                     3
        out     0A1h, al
        mov     ax, [bp+A_FM]
        out     0A0h, al
        mov     al, ah
        out     0A0h, al
        mov     al, [bp+A_MODE]         ; WDAT                        1
        or      al, 20h
        out     0A1h, al
        HOLEN                           ; Wort 0                      2 = 10
        out     0A0h, al
        mov     al, ah
        out     0A0h, al
        mov     di, [bp+A_N]
        dec     di
        jz      fertig
        dec     di                      ; DI = Zahl der Mittelworte
        jz      letzt
        mov     al, 4Ah                 ; MASK FFFFh, WDAT            4 = 14
        out     0A1h, al
        mov     al, 0FFh
        out     0A0h, al
        out     0A0h, al
        mov     al, [bp+A_MODE]
        or      al, 20h
        out     0A1h, al
        mov     ah, 1                   ; noch 1 Wort frei im Buendel
        push    cx
        mov     cx, di                  ; CX = Mittelworte
        mov     di, [bp+A_B]
        and     di, 7
        shl     di, 1
        mov     dl, ah                  ; DL = freie Worte im Buendel
        jmp     word ptr cs:[mtab+di]
mzurueck:
        pop     cx
letzt:  WLEER
        mov     al, 4Ah                 ; MASK lm, WDAT, letztes Wort 6
        out     0A1h, al
        mov     ax, [bp+A_LM]
        out     0A0h, al
        mov     al, ah
        out     0A0h, al
        mov     al, [bp+A_MODE]
        or      al, 20h
        out     0A1h, al
        HOLEN
        out     0A0h, al
        mov     al, ah
        out     0A0h, al
fertig: pop     ds
        pop     di
        pop     si
        pop     bp
        ret

; Mittelworte, je Verschiebung eine Schleife. CX Zahl, DL frei im Buendel,
; BX inv, DS:SI Quelle. DH dient als Zwischenspeicher fuer den Zaehler.
MITTE   MACRO   nb
        LOCAL   lp, ok
lp:     or      dl, dl
        jnz     ok
        WLEER
        mov     dl, 8
ok:     mov     dh, dl
        mov     ax, [si]
        mov     di, [si+2]
        xor     ax, bx
        xor     di, bx
        REPT    nb
        shr     di, 1
        rcr     ax, 1
        ENDM
        out     0A0h, al
        mov     al, ah
        out     0A0h, al
        add     si, 2
        mov     dl, dh
        dec     dl
        loop    lp
        jmp     mzurueck
        ENDM

m0:     MITTE   0
m1:     MITTE   1
m2:     MITTE   2
m3:     MITTE   3
m4:     MITTE   4
m5:     MITTE   5
m6:     MITTE   6
m7:     MITTE   7

mtab    dw      m0, m1, m2, m3, m4, m5, m6, m7
_blit_row ENDP

_TEXT   ENDS
        END

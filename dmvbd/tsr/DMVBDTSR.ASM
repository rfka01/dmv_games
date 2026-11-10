; DMVBDTSR.ASM - startet DMVBD, wenn am DOS-Prompt eine Zeit lang keine
; Taste gedrueckt wurde. Code: Claude, Prompts: rfka01 - MIT-Lizenz
;
; Arbeitsweise (nur DOS, keine Hardware-Interrupts; laeuft deshalb mit
; K230/K231 ohne 8259 ebenso wie mit K235, unter NCR-DOS 2.11 und DOS 3.3):
;   Das TSR haengt sich an INT 21h. Ruft COMMAND.COM (der Prozess, aus dem
;   DMVBDTSR geladen wurde) am Prompt Funktion 0Ah auf, wartet das TSR
;   selbst: Eingabestatus (0Bh), Uhrzeit (2Ch) und INT 28h fuer andere TSRs.
;   Kommt eine Taste, geht der Aufruf unveraendert an DOS - die Taste
;   bleibt im Puffer. Ist die Zeit um, gibt das TSR COMMAND die Zeile
;   "<pfad>DMVBD.EXE /T ..." zurueck, als waere sie getippt worden.
;   COMMAND startet DMVBD also ganz normal; /T loescht beim Ende den
;   Textschirm, danach kommt der neue Prompt.
;   Eine angefangene Zeile ist nie betroffen: die erste Taste beendet das
;   Warten fuer diese Zeile.
;
; Aufruf:  DMVBDTSR [n | nS] [DMVBD-Angaben]   n Minuten (Vorgabe 5), nS Sekunden
;          DMVBDTSR /U                          wieder entfernen
; Ist das TSR schon geladen, uebernimmt es Zeit und Angaben neu.
;
; Bauen: jwasm -bin -Fo=DMVBDTSR.COM DMVBDTSR.ASM

        .model tiny
        .8086
        .code
        org 100h

start:  jmp     install

; ---- residenter Teil -------------------------------------------------
        org     103h
sig     db      'DMVBDTSR'              ; Kennung, fest bei 103h
old21   dd      0
cmdpsp  dw      0                       ; PSP von COMMAND
limit   dw      300                     ; Wartezeit in Sekunden
lastsec db      0
cmdlen  db      0
cmdtxt  db      124 dup (0)             ; Befehlszeile ohne CR

dos:    pushf                           ; DOS am eigenen Haken vorbei
        call    dword ptr cs:[old21]
        ret

new21:  cmp     ah, 0Ah
        je      zeile
chain:  jmp     dword ptr cs:[old21]

zeile:  push    ax
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    es
        mov     si, dx                  ; DS:SI = Eingabepuffer
        cmp     byte ptr [si], 100      ; nur die Befehlszeile (128 Zeichen),
        jb      weiter                  ; nicht DATE/TIME
        mov     ah, 51h                 ; wer ruft? (ausserhalb von DOS erlaubt)
        call    dos
        cmp     bx, cs:[cmdpsp]
        jne     weiter
        sti
        mov     ah, 2Ch
        call    dos
        mov     cs:[lastsec], dh
        xor     di, di                  ; Sekunden seit dem Prompt
warte:  mov     ah, 0Bh                 ; Taste da?
        call    dos
        or      al, al
        jnz     weiter                  ; ja: Zeile normal an DOS
i28:    int     28h                     ; DOS-Leerlauf fuer andere TSRs
        mov     ah, 2Ch
        call    dos
        mov     al, dh
        sub     al, cs:[lastsec]
        jnc     @F
        add     al, 60
@@:     mov     cs:[lastsec], dh
        xor     ah, ah
        add     di, ax
        cmp     di, cs:[limit]
        jb      warte
        ; Zeit um: Befehlszeile zurueckgeben
        mov     cl, cs:[cmdlen]
        mov     al, [si]
        dec     al                      ; Platz fuer CR
        cmp     cl, al
        jbe     @F
        mov     cl, al
@@:     xor     ch, ch
        mov     [si+1], cl
        push    ds
        pop     es
        lea     di, [si+2]
        push    ds
        push    cs
        pop     ds
        mov     si, offset cmdtxt
        cld
        rep     movsb
        pop     ds
        mov     byte ptr es:[di], 13
        pop     es
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        iret

weiter: pop     es
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        jmp     chain

; nur fuer den Test (/TEST): ersetzt DOS
stub:   int     21h
        iret

resend  label   byte

; ---- Laden -----------------------------------------------------------
m_ok    db      'DMVBDTSR geladen: DMVBD startet nach $'
m_neu   db      'DMVBDTSR war schon geladen; neu: DMVBD startet nach $'
m_sek   db      ' s ohne Taste am DOS-Prompt.', 13, 10, '$'
m_weg   db      'DMVBDTSR entfernt.', 13, 10, '$'
m_nicht db      'DMVBDTSR ist nicht geladen - oder nach ihm wurde ein anderes'
        db      ' Programm an INT 21h geladen,', 13, 10
        db      'dann muss dieses zuerst entfernt werden.', 13, 10, '$'
m_hilfe db      'DMVBDTSR [n | nS] [DMVBD-Angaben]  - DMVBD nach n Minuten'
        db      ' (nS: Sekunden) ohne Taste am DOS-Prompt', 13, 10
        db      'DMVBDTSR /U                        - wieder entfernen', 13, 10
        db      'Code: Claude, Prompts: rfka01', 13, 10, '$'
s_dmvbd db      'DMVBD.EXE /T', 0
testbuf db      128, 0, 128 dup (0)
testmod db      0
dosver  db      0

install:
        cld
        mov     ah, 30h
        int     21h
        mov     [dosver], al
        mov     si, 81h
        call    blank
        cmp     byte ptr [si], '/'
        jne     zeit
        mov     al, [si+1]
        and     al, 0DFh
        cmp     al, 'U'
        je      entfernen
        cmp     byte ptr [si+1], '?'
        mov     dx, offset m_hilfe
        je      ausgabe_ende
        cmp     word ptr [si+1], 'ET'   ; "/TEST" (nur zum Pruefen)
        jne     zeit
        cmp     word ptr [si+3], 'TS'
        jne     zeit
        mov     [testmod], 1
        add     si, 5
        call    blank

zeit:   ; Zahl: Minuten, mit S: Sekunden
        xor     bx, bx
        mov     cx, 0
z1:     mov     al, [si]
        sub     al, '0'
        cmp     al, 9
        ja      z2
        inc     cx
        xor     ah, ah
        push    ax
        mov     ax, 10
        mul     bx
        pop     bx
        add     bx, ax
        inc     si
        jmp     z1
z2:     jcxz    vorgabe
        mov     al, [si]
        and     al, 0DFh
        cmp     al, 'S'
        jne     minuten
        inc     si
        jmp     z3
minuten:
        cmp     bx, 1000
        jb      @F
        mov     bx, 999
@@:     mov     ax, 60
        mul     bx
        mov     bx, ax
        jmp     z3
vorgabe:
        mov     bx, 300
z3:     or      bx, bx
        jnz     @F
        inc     bx
@@:     mov     [limit], bx
        call    blank
        call    befehl                  ; cmdtxt/cmdlen aufbauen (SI = Rest)

        cmp     [testmod], 0
        jne     test2

        call    finden                  ; ES = residente Kopie oder 0
        mov     ax, es
        or      ax, ax
        jz      neu
        ; schon geladen: Zeit und Befehl uebernehmen
        mov     ax, [limit]
        mov     es:[limit], ax
        mov     si, offset cmdlen
        mov     di, si
        mov     cx, 1 + 124
        rep     movsb
        mov     dx, offset m_neu
        call    text
        call    zeit_zeigen
        mov     ax, 4C00h
        int     21h

neu:    mov     ax, word ptr ds:[16h]   ; aufrufender Prozess = COMMAND
        mov     [cmdpsp], ax
        mov     ax, 3521h
        int     21h
        mov     word ptr [old21], bx
        mov     word ptr [old21+2], es
        mov     dx, offset new21
        mov     ax, 2521h
        int     21h
        mov     es, word ptr ds:[2Ch]   ; Umgebung freigeben
        mov     ah, 49h
        int     21h
        mov     word ptr ds:[2Ch], 0
        mov     dx, offset m_ok
        call    text
        call    zeit_zeigen
        mov     dx, (resend - start + 100h + 15) / 16
        mov     ax, 3100h
        int     21h

; Test ohne DOS-Vektoren: ein 0Ah-Aufruf direkt an new21
test2:  mov     word ptr [old21], offset stub
        mov     word ptr [old21+2], cs
        mov     ah, 51h
        int     21h
        mov     [cmdpsp], bx
        mov     word ptr [i28], 9090h   ; INT 28h nicht im Test
        mov     dx, offset testbuf
        mov     ah, 0Ah
        pushf
        push    cs
        call    new21
        mov     bl, [testbuf+1]
        xor     bh, bh
        mov     byte ptr [testbuf+2+bx], '$'
        mov     dx, offset testbuf+2
        call    text
        mov     ax, 4C00h
        int     21h

entfernen:
        call    finden
        mov     ax, es
        or      ax, ax
        mov     dx, offset m_nicht
        jz      ausgabe_ende
        push    ds
        lds     dx, es:[old21]
        mov     ax, 2521h
        int     21h
        pop     ds
        mov     ah, 49h                 ; ES = residenter Block
        int     21h
        mov     dx, offset m_weg
ausgabe_ende:
        call    text
        mov     ax, 4C00h
        int     21h

; residente Kopie suchen: zeigt INT 21h auf new21 in einem Block mit
; unserer Kennung, ist es unsere Kopie. ES = Segment oder 0.
finden: push    si
        push    di
        push    cx
        push    bx
        mov     ax, 3521h
        int     21h
        cmp     bx, offset new21
        jne     fn_nein
        mov     si, offset sig
        mov     di, si
        mov     cx, 8
        repe    cmpsb
        je      fn_ja
fn_nein:
        xor     ax, ax
        mov     es, ax
fn_ja:  pop     bx
        pop     cx
        pop     di
        pop     si
        ret

blank:  cmp     byte ptr [si], ' '
        je      bl1
        cmp     byte ptr [si], 9
        jne     bl2
bl1:    inc     si
        jmp     blank
bl2:    ret

; cmdtxt = <Verzeichnis von DMVBDTSR>DMVBD.EXE /T [Rest der Zeile]
befehl: push    si
        mov     di, offset cmdtxt
        push    ds
        pop     es
        cmp     [dosver], 3             ; Programmpfad erst ab DOS 3
        jb      bf_name
        push    ds
        mov     ds, word ptr ds:[2Ch]
        xor     si, si
bf1:    lodsb                           ; Ende der Umgebung: 0 0
        or      al, al
        jnz     bf1
        lodsb
        or      al, al
        jnz     bf1
        lodsw                           ; Zahl der Zusaetze
        mov     bx, di                  ; hinter dem letzten \ oder :
bf2:    lodsb
        or      al, al
        jz      bf3
        stosb
        cmp     al, '\'
        je      bf2a
        cmp     al, ':'
        jne     bf2b
bf2a:   mov     bx, di
bf2b:   cmp     di, offset cmdtxt + 80
        jb      bf2
bf3:    pop     ds
        mov     di, bx
bf_name:
        mov     si, offset s_dmvbd
bf4:    lodsb
        or      al, al
        jz      bf5
        stosb
        jmp     bf4
bf5:    pop     si
        cmp     byte ptr [si], 13
        je      bf7
        mov     al, ' '
        stosb
bf6:    lodsb
        cmp     al, 13
        je      bf7
        cmp     di, offset cmdtxt + 123
        jae     bf7
        stosb
        jmp     bf6
bf7:    mov     ax, di
        sub     ax, offset cmdtxt
        mov     [cmdlen], al
        ret

text:   mov     ah, 9
        int     21h
        ret

; Wartezeit in Sekunden als Zahl ausgeben
zeit_zeigen:
        mov     ax, [limit]
        mov     bx, 10
        xor     cx, cx
zz1:    xor     dx, dx
        div     bx
        push    dx
        inc     cx
        or      ax, ax
        jnz     zz1
zz2:    pop     dx
        add     dl, '0'
        mov     ah, 2
        int     21h
        loop    zz2
        mov     dx, offset m_sek
        jmp     text

        end     start

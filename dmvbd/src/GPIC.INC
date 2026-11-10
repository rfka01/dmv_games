; GPIC.INC - gepackte Vollbilder (.PIC, Kennung DMVP, LZSS) direkt in den
; Bildspeicher der NCR DMV. MASM 5.1 (auch JWasm -Zm), 8086, eigenstaendig:
; alle Namen mit gp_, keine Variablen des Programms.
;
; Dieselbe Quelle liefert den Kern der DMVGRAF-Bibliothek (MS-DOS):
; tools/gpicbin.py uebersetzt src/GPICB.ASM (GP_NURKERN, ORG 0) mit JWasm
; nach src/GPIC86.S. Der Kern ist lageunabhaengig (nur relative Spruenge,
; Daten im Arbeitsbereich ueber DS).
;
; Aufruf gp_pic (Arbeitsbereich gp_puf in dieser Datei, GP_GROESSE = 5311 Byte):
;   DS:DX Dateiname (ASCIIZ), AL art, BX xw (Spalte in Worten 0-39),
;   CX y (Zeile 0-399). Rueck: CF = 1 Fehler, AX = Nummer; sonst AX = 0.
; Aufruf gp_picw: dasselbe, ES:DI = Arbeitsbereich des Aufrufers
;   (mindestens GP_GROESSE Byte, etwa aus FREEMEM).
; FIFO: warten auf "leer", dann 16 Byte ohne Abfrage (schneller als vor jedem
;   Byte auf "nicht voll" zu warten).
; art: Bit 0 Anzeige waehrend des Schreibens dunkel, Bit 1 Mono, Bit 2
;   Farbe (ohne beides: Farbe), Bit 7 Quelle im Speicher: DS:DX = Daten,
;   SI = Laenge in Byte.
; Fehler: 92 Datei fehlt/nicht lesbar, 93 zu kurz oder Ebenenlaenge
;   stimmt nicht, 94 unbekanntes Format oder Version, 96 Bild ragt ueber
;   den Schirm, 97 Verweis vor den Anfang der Ebene.
; Alle anderen Register bleiben erhalten; DF wird geloescht.
;
; Code: Claude (Anthropic), Idee und Prompts: rfka01. Lizenz: MIT

GP_GDCS  EQU 0A0h               ; Status lesen, Parameter schreiben
GP_GDCB  EQU 0A1h               ; Befehl schreiben

; Arbeitsbereich (auf Absatzgrenze gelegt, Versatz 0 = Anfang)
GP_HAND  EQU 0                  ; Dateinummer, FFFFh = Speicher
GP_SRCO  EQU 2                  ; Speicher: aktueller Zeiger, Rest
GP_SRCS  EQU 4
GP_REST  EQU 6
GP_ORGO  EQU 8                  ; Speicher: Anfang und Laenge
GP_ORGS  EQU 10
GP_ORGL  EQU 12
GP_ART   EQU 14
GP_XW    EQU 16
GP_Y     EQU 18
GP_BR    EQU 20                 ; Breite in Worten
GP_HO    EQU 22                 ; Hoehe
GP_N     EQU 24                 ; Ebenen in der Datei
GP_EP    EQU 26                 ; Ende des nutzbaren Puffers
GP_BE    EQU 28                 ; Ende der gelesenen Daten
GP_PK    EQU 30                 ; Byte der Ebene, die noch nicht im Puffer sind
GP_SP0   EQU 32                 ; SP fuer Fehler
GP_SP1   EQU 34                 ; SP fuer Ebenenende
GP_TOT   EQU 36                 ; Byte je Ebene
GP_RLEN  EQU 38                 ; Byte je Zeilenstueck
GP_RADR  EQU 40                 ; Wortadresse der Zeile ohne Ebene
GP_BASE  EQU 42                 ; Ebenenadresse
GP_OFFL  EQU 44                 ; Lage der Ebene in der Datei
GP_OFFH  EQU 46
GP_I     EQU 48                 ; Eintrag
GP_ZIEL  EQU 50                 ; Zielebenen des Eintrags (Bits G R B)
GP_HAT   EQU 52                 ; welche Arten die Datei hat (Bits G R B M)
GP_GES   EQU 54                 ; geschriebene Farbebenen
GP_TAB   EQU 56                 ; 4 Eintraege zu 4 Byte (bis 71)
GP_KOPF  EQU 72                 ; 12 Byte Kopf (bis 83)
GP_SIQ   EQU 84                 ; Lesezeiger waehrend eines Verweises
GP_INV   EQU 86                 ; 1 = im Verweis (SI ist dann Fensterstelle)
GP_ZL    EQU 88                 ; Zeilenstuecke, die noch kommen
GP_DONE  EQU 90                 ; ausgegebene Byte vor dem laufenden Stueck
GP_NZ    EQU 92                 ; Zeilenstuecke je Ebene (H, volle Breite 1)
GP_MULT  EQU 94                 ; 1 = eine Ebene in mehrere (Zeilenpuffer)
GP_RBUF  EQU 96
GP_RBLEN EQU 1024
GP_WIN   EQU GP_RBUF + GP_RBLEN ; Fenster 4 KB
GP_ROWB  EQU GP_WIN + 4096      ; Zeilenpuffer 80 Byte (rueckwaerts gefuellt)
GP_GROESSE EQU GP_ROWB + 80 + 15

; ---- Hilfen ---------------------------------------------------------

gp_bef  MACRO                   ; AL als Befehl, vorher FIFO abwarten
        LOCAL w
        mov     ah, al
w:      in      al, GP_GDCS
        test    al, 2
        jnz     w
        mov     al, ah
        out     GP_GDCB, al
        ENDM

gp_par  MACRO                   ; AL als Parameter
        LOCAL w
        mov     ah, al
w:      in      al, GP_GDCS
        test    al, 2
        jnz     w
        mov     al, ah
        out     GP_GDCS, al
        ENDM

gp_getb MACRO                   ; naechstes Byte der Ebene nach AL
        LOCAL g
        cmp     si, ds:[GP_EP]
        jb      g
        call    gp_fill
g:      lodsb
        ENDM

gp_putb MACRO                   ; AL ausgeben, ins Fenster; Ende -> gp_ende
        LOCAL z
        mov     [bx+GP_WIN], al
        inc     bx
        and     bh, 0Fh
        gp_out
        ENDM

gp_out  MACRO                   ; AL an den 7220; Zeilenende -> gp_zeile
        LOCAL z
        gp_fifo
        out     GP_GDCS, al
        dec     di
        jnz     z
        call    gp_zeile
z:
        ENDM

; FIFO in Bloecken: ist es leer, passen 16 Byte ohne Abfrage. BP zaehlt
; die freien Plaetze; nach Befehlen (gp_bef/gp_par) ist BP = -1.
gp_fifo MACRO
        LOCAL w, ok
        dec     bp
        jns     ok
        mov     ah, al
w:      in      al, GP_GDCS
        test    al, 4
        jz      w
        mov     al, ah
        mov     bp, 15
ok:
        ENDM

gp_mput MACRO                   ; mehrere Ebenen: ins Fenster und in den
        mov     [bx+GP_WIN], al ; Zeilenpuffer; Zeilenende -> gp_zeilm
        inc     bx
        and     bh, 0Fh
        gp_mout
        ENDM

gp_mout MACRO
        LOCAL z
        mov     [di+GP_ROWB-1], al
        dec     di
        jnz     z
        call    gp_zeilm
z:
        ENDM

; Entpacken (LZSS) mit Ausgabe put; Etiketten je Fassung eigen
gp_lzss MACRO   put
        LOCAL l1, l2, ver, v0, v1, f97
        xor     dx, dx
l1:     dec     dh
        jns     l2
        gp_getb
        mov     dl, al
        mov     dh, 7
l2:     shr     dl, 1
        jc      ver
        gp_getb
        put
        jmp     l1
ver:    gp_getb
        mov     cl, al                  ; Abstand Bits 0-7
        gp_getb
        mov     ch, al
        push    dx
        push    si
        mov     ds:[GP_SIQ], si
        mov     byte ptr ds:[GP_INV], 1
        mov     dl, ch
        and     dl, 0Fh
        add     dl, 3                   ; Laenge
        shr     ch, 1
        shr     ch, 1
        shr     ch, 1
        shr     ch, 1                   ; CX = Abstand
        mov     ax, ds:[GP_DONE]        ; schon ausgegeben
        add     ax, ds:[GP_RLEN]
        sub     ax, di
        cmp     cx, ax
        ja      f97
        or      cx, cx
        jnz     v0
f97:    jmp     gp_f97
v0:     mov     si, bx
        sub     si, cx
        and     si, 0FFFh
        mov     cl, dl
        xor     ch, ch
v1:     mov     al, [si+GP_WIN]
        inc     si
        and     si, 0FFFh
        put
        loop    v1
        pop     si
        pop     dx
        mov     byte ptr ds:[GP_INV], 0
        jmp     l1
        ENDM

IFNDEF GP_NURKERN
gp_pic  PROC    NEAR
        push    es
        push    di
        push    ds
        pop     es
        mov     di, OFFSET gp_puf
        call    gp_picw
        pop     di
        pop     es
        ret
gp_pic  ENDP
ENDIF

; ---- Einsprung --------------------------------------------------------

gp_picw PROC    NEAR
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    bp
        push    ds
        push    es
        cld
        push    cx                      ; y
        mov     bp, di                  ; Arbeitsbereich auf Absatzgrenze
        add     bp, 15
        mov     cl, 4
        shr     bp, cl
        mov     cx, es
        add     bp, cx
        mov     es, bp
        pop     cx
        mov     es:[GP_ART], al
        test    al, 1                   ; Anzeige aus, noch vor dem Oeffnen
        jz      gp_k00                  ; (danach wie in DMVGRAF: in MAME
        push    ax                      ; blieb sie sonst manchmal dunkel)
        mov     al, 0Ch
        gp_bef
        pop     ax
gp_k00: mov     es:[GP_XW], bx
        mov     es:[GP_Y], cx
        mov     word ptr es:[GP_HAND], 0FFFFh
        test    al, 80h
        jz      gp_dat
        mov     es:[GP_SRCO], dx        ; Quelle im Speicher
        mov     es:[GP_ORGO], dx
        mov     es:[GP_SRCS], ds
        mov     es:[GP_ORGS], ds
        mov     es:[GP_REST], si
        mov     es:[GP_ORGL], si
        jmp     short gp_ws
gp_dat: mov     ax, 3D00h               ; Datei oeffnen
        int     21h
        jnc     gp_auf
        push    es                      ; Datei fehlt: Anzeige wieder an
        pop     ds
        mov     ax, 92
        stc
        jmp     gp_schl
gp_auf: mov     es:[GP_HAND], ax
gp_ws:  push    es
        pop     ds
        mov     ds:[GP_SP0], sp
gp_k0:
; Kopf lesen
        xor     ax, ax
        xor     dx, dx
        call    gp_geh
        mov     word ptr ds:[GP_PK], 12
        call    gp_setep
        mov     di, GP_KOPF
        mov     cx, 12
gp_k1:  gp_getb
        mov     [di], al
        inc     di
        loop    gp_k1
        cmp     word ptr ds:[GP_KOPF], 'MD'
        jne     gp_alt
        cmp     word ptr ds:[GP_KOPF+2], 'PV'
        jne     gp_alt
        cmp     byte ptr ds:[GP_KOPF+4], 1      ; Version
        jne     gp_f94
        mov     al, ds:[GP_KOPF+5]
        xor     ah, ah
        mov     ds:[GP_N], ax
        dec     ax
        cmp     ax, 4
        jae     gp_f94
        mov     ax, ds:[GP_KOPF+6]
        mov     ds:[GP_BR], ax
        dec     ax
        cmp     ax, 40
        jae     gp_f94
        mov     ax, ds:[GP_KOPF+8]
        mov     ds:[GP_HO], ax
        dec     ax
        cmp     ax, 400
        jae     gp_f94
        mov     ax, ds:[GP_N]           ; Tabelle lesen
        shl     ax, 1
        shl     ax, 1
        mov     ds:[GP_PK], ax
        mov     cx, ax
        call    gp_setep
        mov     di, GP_TAB
gp_k2:  gp_getb
        mov     [di], al
        inc     di
        loop    gp_k2
        mov     ax, ds:[GP_N]
        shl     ax, 1
        shl     ax, 1
        add     ax, 12
        mov     ds:[GP_OFFL], ax
        mov     word ptr ds:[GP_OFFH], 0
        jmp     gp_pruef
gp_f94: mov     ax, 94
        jmp     gp_fehl
; alte Rohdatei: 128000 Byte = G R B M, 32000 Byte = M, je 40 x 400
gp_alt: cmp     word ptr ds:[GP_HAND], 0FFFFh
        je      gp_alts
        mov     ax, 4202h
        mov     bx, ds:[GP_HAND]
        xor     cx, cx
        xor     dx, dx
        int     21h
        jc      gp_f94
        jmp     short gp_alt2
gp_alts:mov     ax, ds:[GP_ORGL]
        xor     dx, dx
gp_alt2:mov     word ptr ds:[GP_TAB], 0047h     ; 'G', roh
        mov     word ptr ds:[GP_TAB+4], 0052h   ; 'R'
        mov     word ptr ds:[GP_TAB+8], 0042h   ; 'B'
        mov     word ptr ds:[GP_TAB+12], 004Dh  ; 'M'
        mov     cx, 4
        mov     bx, GP_TAB
        cmp     dx, 1                   ; 128000 = 1F400h
        jne     gp_alt3
        cmp     ax, 0F400h
        je      gp_alt4
gp_alt3:or      dx, dx
        jnz     gp_f94
        cmp     ax, 32000
        jne     gp_f94
        mov     cx, 1
        mov     bx, GP_TAB+12
        mov     word ptr ds:[GP_TAB], 004Dh
gp_alt4:mov     ds:[GP_N], cx
        mov     word ptr ds:[GP_BR], 40
        mov     word ptr ds:[GP_HO], 400
        mov     di, GP_TAB
gp_alt5:mov     word ptr [di+2], 32000
        add     di, 4
        loop    gp_alt5
        mov     word ptr ds:[GP_OFFL], 0
        mov     word ptr ds:[GP_OFFH], 0
; Lage pruefen, Arten und Verfahren pruefen
gp_pruef:
        mov     ax, ds:[GP_XW]
        add     ax, ds:[GP_BR]
        cmp     ax, 40
        ja      gp_f96
        mov     ax, ds:[GP_Y]
        add     ax, ds:[GP_HO]
        cmp     ax, 400
        jbe     gp_p0
gp_f96: mov     ax, 96
        jmp     gp_fehl
gp_p0:  mov     cx, ds:[GP_N]
        mov     di, GP_TAB
        xor     dx, dx                  ; DL: vorhandene Arten
gp_p1:  mov     al, [di]
        mov     ah, 1
        cmp     al, 'G'
        je      gp_p2
        mov     ah, 2
        cmp     al, 'R'
        je      gp_p2
        mov     ah, 4
        cmp     al, 'B'
        je      gp_p2
        mov     ah, 8
        cmp     al, 'M'
        jne     gp_f94b
gp_p2:  or      dl, ah
        cmp     byte ptr [di+1], 2
        jae     gp_f94b
        add     di, 4
        loop    gp_p1
        mov     ds:[GP_HAT], dx
        mov     ax, ds:[GP_BR]          ; Byte je Ebene, je Zeilenstueck
        shl     ax, 1
        mov     ds:[GP_RLEN], ax
        mul     word ptr ds:[GP_HO]
        mov     ds:[GP_TOT], ax
        mov     al, 4Ah                 ; MASK FFFF, FIGS 02 00 00
        gp_bef
        mov     al, 0FFh
        gp_par
        gp_par
        mov     al, 4Ch
        gp_bef
        mov     al, 2
        gp_par
        xor     al, al
        gp_par
        gp_par
        mov     word ptr ds:[GP_GES], 0
        mov     word ptr ds:[GP_I], 0
        jmp     short gp_e0
gp_f94b:jmp     gp_f94
; ---- je Eintrag der Tabelle ----
gp_e0:  mov     di, ds:[GP_I]
        shl     di, 1
        shl     di, 1
        add     di, GP_TAB
        call    gp_wohin                ; AL: Zielebenen (G=1 R=2 B=4)
        mov     ds:[GP_ZIEL], ax
        mov     byte ptr ds:[GP_MULT], 0
        cmp     al, 7                   ; M in alle drei: einmal entpacken,
        jne     gp_e00                  ; Zeile fuer Zeile in jede Ebene
        mov     byte ptr ds:[GP_MULT], 1
        or      ds:[GP_GES], al
        mov     word ptr ds:[GP_ZIEL], 0
        mov     bx, ds:[GP_I]
        shl     bx, 1
        shl     bx, 1
        mov     al, ds:[bx+GP_TAB+1]
        mov     cx, ds:[bx+GP_TAB+2]
        call    gp_ebene
        mov     byte ptr ds:[GP_MULT], 0
gp_e00: mov     cx, 0                   ; Ebene 0 (G) .. 2 (B)
gp_e1:  test    byte ptr ds:[GP_ZIEL], 1
        jz      gp_e2
        push    cx
        mov     ax, cx                  ; Ebenenadresse 0000h/4000h/8000h
        mov     ah, al
        xor     al, al
        shl     ah, 1
        shl     ah, 1
        shl     ah, 1
        shl     ah, 1
        shl     ah, 1
        shl     ah, 1
        mov     ds:[GP_BASE], ax
        mov     bx, ds:[GP_I]
        shl     bx, 1
        shl     bx, 1
        mov     al, ds:[bx+GP_TAB+1]    ; Verfahren
        mov     cx, ds:[bx+GP_TAB+2]    ; gepackte Laenge
        call    gp_ebene
        pop     cx
        mov     al, 1
        shl     al, cl
        or      ds:[GP_GES], al
gp_e2:  shr     word ptr ds:[GP_ZIEL], 1
        inc     cx
        cmp     cx, 3
        jb      gp_e1
        mov     bx, ds:[GP_I]           ; naechster Eintrag: Lage + Laenge
        shl     bx, 1
        shl     bx, 1
        mov     ax, ds:[bx+GP_TAB+2]
        add     ds:[GP_OFFL], ax
        adc     word ptr ds:[GP_OFFH], 0
        inc     word ptr ds:[GP_I]
        mov     ax, ds:[GP_I]
        cmp     ax, ds:[GP_N]
        jb      gp_e0
; Farbe: fehlende Farbebenen loeschen, wenn die Datei G, R oder B hat
        test    byte ptr ds:[GP_ART], 2
        jnz     gp_fertig
        test    byte ptr ds:[GP_HAT], 7
        jz      gp_fertig
        mov     cx, 0
gp_l1:  mov     al, 1
        shl     al, cl
        test    ds:[GP_GES], al
        jnz     gp_l2
        push    cx
        mov     ax, cx
        mov     ah, al
        xor     al, al
        shl     ah, 1
        shl     ah, 1
        shl     ah, 1
        shl     ah, 1
        shl     ah, 1
        shl     ah, 1
        mov     ds:[GP_BASE], ax
        mov     al, 2                   ; Verfahren 2: Nullen
        call    gp_ebene
        pop     cx
gp_l2:  inc     cx
        cmp     cx, 3
        jb      gp_l1
gp_fertig:
        xor     ax, ax
        jmp     short gp_schl
gp_fehl:mov     sp, ds:[GP_SP0]
        stc
gp_schl:pushf
        push    ax
        cmp     word ptr ds:[GP_HAND], 0FFFFh
        je      gp_s1
        mov     ah, 3Eh
        mov     bx, ds:[GP_HAND]
        int     21h
gp_s1:  test    byte ptr ds:[GP_ART], 1
        jz      gp_s2
        mov     al, 6Bh                 ; START und Anzeige an (wie NCRGRAPH)
        gp_bef
        mov     al, 0Dh
        gp_bef
gp_s2:  pop     ax
        popf
gp_raus:pop     es
        pop     ds
        pop     bp
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        ret
gp_picw ENDP

; Zielebenen fuer den Eintrag an [DI]: AX = Bits G R B
gp_wohin PROC   NEAR
        mov     al, [di]
        mov     dl, ds:[GP_HAT]
        test    byte ptr ds:[GP_ART], 2
        jnz     gp_zm
        mov     ah, 1                   ; Farbe
        cmp     al, 'G'
        je      gp_z1
        mov     ah, 2
        cmp     al, 'R'
        je      gp_z1
        mov     ah, 4
        cmp     al, 'B'
        je      gp_z1
        mov     ah, 7                   ; M: in alle drei, wenn kein G R B
        test    dl, 7
        jz      gp_z1
gp_z0:  xor     ax, ax
        ret
gp_zm:  mov     ah, 1                   ; Mono: M nach Gruen, sonst G
        cmp     al, 'M'
        je      gp_z1
        cmp     al, 'G'
        jne     gp_z0
        test    dl, 8
        jnz     gp_z0
gp_z1:  mov     al, ah
        xor     ah, ah
        ret
gp_wohin ENDP

; ---- Lesen ----------------------------------------------------------------

; an die Stelle DX:AX der Quelle gehen, Puffer leeren
gp_geh  PROC    NEAR
        cmp     word ptr ds:[GP_HAND], 0FFFFh
        je      gp_g1
        mov     cx, dx
        mov     dx, ax
        mov     ax, 4200h
        mov     bx, ds:[GP_HAND]
        int     21h
        jnc     gp_g2
        mov     ax, 92
        jmp     gp_fehl
gp_g1:  or      dx, dx                  ; Speicher: hoechstens 64 KB
        jnz     gp_g3
        mov     cx, ds:[GP_ORGL]
        sub     cx, ax
        jb      gp_g3
        mov     ds:[GP_REST], cx
        mov     dx, ds:[GP_ORGS]
        add     ax, ds:[GP_ORGO]
        jnc     gp_g4
        add     dx, 1000h
gp_g4:  call    gp_norm
gp_g2:  mov     si, GP_RBUF
        mov     ds:[GP_BE], si
        mov     ds:[GP_EP], si
        ret
gp_g3:  mov     ax, 93
        jmp     gp_fehl
gp_geh  ENDP

; DX:AX (Segment:Versatz) klein machen und als Speicherzeiger ablegen
gp_norm PROC    NEAR
        push    ax
        push    cx
        mov     cl, 4
        shr     ax, cl
        add     dx, ax
        pop     cx
        pop     ax
        and     ax, 15
        mov     ds:[GP_SRCO], ax
        mov     ds:[GP_SRCS], dx
        ret
gp_norm ENDP

; nutzbares Pufferende setzen: hoechstens GP_PK weitere Byte der Ebene
gp_setep PROC   NEAR
        push    ax
        push    cx
        mov     ax, ds:[GP_BE]
        sub     ax, si
        mov     cx, ds:[GP_PK]
        cmp     ax, cx
        jbe     gp_se1
        mov     ax, cx
gp_se1: sub     ds:[GP_PK], ax
        add     ax, si
        mov     ds:[GP_EP], ax
        pop     cx
        pop     ax
        ret
gp_setep ENDP

; Puffer leer (SI = EP): nachlesen; Ebene zu Ende -> Fehler 93
gp_fill PROC    NEAR
        cmp     word ptr ds:[GP_PK], 0
        je      gp_fz
        cmp     si, ds:[GP_BE]
        jb      gp_f9
        push    ax
        push    bx
        push    cx
        push    dx
        push    di
        cmp     word ptr ds:[GP_HAND], 0FFFFh
        je      gp_fm
        mov     ah, 3Fh
        mov     bx, ds:[GP_HAND]
        mov     cx, GP_RBLEN
        mov     dx, GP_RBUF
        int     21h
        jc      gp_fd
        jmp     short gp_f8
gp_fm:  mov     cx, ds:[GP_REST]        ; aus dem Speicher
        cmp     cx, GP_RBLEN
        jbe     gp_f7
        mov     cx, GP_RBLEN
gp_f7:  sub     ds:[GP_REST], cx
        push    ds
        push    es
        push    cx
        push    ds
        pop     es
        mov     di, GP_RBUF
        mov     si, ds:[GP_SRCO]
        mov     ds, ds:[GP_SRCS]
        rep     movsb
        pop     ax
        pop     es
        pop     ds
        push    ax
        mov     dx, ds:[GP_SRCS]
        mov     ax, si
        call    gp_norm
        pop     ax
gp_f8:  or      ax, ax
        jz      gp_fz
        mov     si, GP_RBUF
        add     ax, si
        mov     ds:[GP_BE], ax
        pop     di
        pop     dx
        pop     cx
        pop     bx
        pop     ax
gp_f9:  jmp     gp_setep
gp_fd:  mov     ax, 92
        jmp     gp_fehl
gp_fz:  mov     ax, 93
        jmp     gp_fehl
gp_fill ENDP

; ---- eine Ebene ------------------------------------------------------------
; AL Verfahren (0 roh, 1 LZSS, 2 Nullen), CX gepackte Laenge,
; GP_BASE Ebenenadresse, GP_OFFH:GP_OFFL Lage in der Datei.
gp_ebene PROC   NEAR
        push    ax
        cmp     al, 2
        je      gp_eb1
        push    cx
        mov     ax, ds:[GP_OFFL]
        mov     dx, ds:[GP_OFFH]
        call    gp_geh
        pop     cx
        mov     ds:[GP_PK], cx
        call    gp_setep
        jmp     short gp_eb2
gp_eb1: mov     word ptr ds:[GP_PK], 0  ; Nullen: nichts lesen
        mov     si, ds:[GP_EP]
gp_eb2: mov     ax, ds:[GP_BR]          ; Zeilenstuecke: je Zeile, bei
        shl     ax, 1                   ; voller Breite (eine Ebene) ein Strom
        mov     cx, ds:[GP_HO]
        cmp     byte ptr ds:[GP_MULT], 0
        jne     gp_eb3
        cmp     word ptr ds:[GP_BR], 40
        jne     gp_eb3
        mov     ax, ds:[GP_TOT]
        mov     cx, 1
gp_eb3: mov     ds:[GP_RLEN], ax
        mov     ds:[GP_ZL], cx
        xor     ax, ax
        sub     ax, ds:[GP_RLEN]
        mov     ds:[GP_DONE], ax
        mov     ax, ds:[GP_Y]           ; Zeilenadresse y * 40 + xw
        mov     cx, 40
        mul     cx
        add     ax, ds:[GP_XW]
        xor     bx, bx
        mov     byte ptr ds:[GP_INV], 0
        cmp     byte ptr ds:[GP_MULT], 0
        je      gp_eb4
        mov     ds:[GP_RADR], ax
        call    gp_zeilm
        pop     ax
        mov     ds:[GP_SP1], sp
        cmp     al, 1
        je      gp_mlz
gp_mroh:gp_getb                         ; roh in mehrere Ebenen
        gp_mout
        jmp     gp_mroh
gp_mlz: gp_lzss gp_mput
gp_eb4: sub     ax, 40                  ; gp_zeile zaehlt erst weiter
        mov     ds:[GP_RADR], ax
        call    gp_zeile
        pop     ax
        mov     ds:[GP_SP1], sp
        cmp     al, 1
        je      gp_lz
        cmp     al, 2
        je      gp_nul
gp_roh: gp_getb                         ; roh: ohne Fenster
        gp_out
        jmp     gp_roh
gp_nul: xor     al, al
        gp_out
        jmp     gp_nul
; LZSS: Flagbyte (Bit 0 zuerst), 0 Literal, 1 Verweis (Abstand 12 Bit,
; Laenge 4 Bit + 3)
gp_lz:  gp_lzss gp_putb
gp_f97: mov     ax, 97
        jmp     gp_fehl
gp_ende:
        mov     sp, ds:[GP_SP1]         ; Ebene voll: Rest pruefen
        cmp     byte ptr ds:[GP_INV], 0
        je      gp_en1
        mov     si, ds:[GP_SIQ]
        mov     byte ptr ds:[GP_INV], 0
gp_en1:
        cmp     word ptr ds:[GP_PK], 0
        jne     gp_f93
        cmp     si, ds:[GP_EP]
        jne     gp_f93
        ret
gp_f93: mov     ax, 93
        jmp     gp_fehl
gp_ebene ENDP

; naechste Zeile: CURS auf GP_BASE + GP_RADR + 40, WDAT; DI = Byte der Zeile
gp_zeile PROC   NEAR
        push    ax
        mov     ax, ds:[GP_RLEN]
        add     ds:[GP_DONE], ax
        dec     word ptr ds:[GP_ZL]
        jns     gp_zl1
        jmp     gp_ende                 ; Ebene voll (SP stellt gp_ende)
gp_zl1: mov     ax, ds:[GP_RADR]
        add     ax, 40
        mov     ds:[GP_RADR], ax
        add     ax, ds:[GP_BASE]
        push    ax
        mov     al, 49h                 ; CURS, zwei Parameter (Punkt bleibt)
        gp_bef
        pop     ax
        push    ax
        gp_par
        pop     ax
        mov     al, ah
        gp_par
        mov     al, 20h                 ; WDAT Wort, ersetzen
        gp_bef
        mov     di, ds:[GP_RLEN]
        mov     bp, -1
        pop     ax
        ret
gp_zeile ENDP

; mehrere Ebenen: Zeilenpuffer (rueckwaerts) in jede Zielebene, dann die
; naechste Zeile; beim ersten Aufruf nur einrichten
gp_zeilm PROC   NEAR
        push    ax
        mov     ax, ds:[GP_RLEN]
        add     ds:[GP_DONE], ax
        cmp     word ptr ds:[GP_DONE], 0
        je      gp_zm3
        push    cx
        push    dx
        push    si
        mov     dx, 0                   ; Ebenen 0000h 4000h 8000h
gp_zm1: mov     al, 49h                 ; CURS
        gp_bef
        mov     ax, ds:[GP_RADR]
        add     ax, dx
        push    ax
        gp_par
        pop     ax
        mov     al, ah
        gp_par
        mov     al, 20h                 ; WDAT
        gp_bef
        mov     cx, ds:[GP_RLEN]
        mov     si, cx
        add     si, GP_ROWB-1
        std
        mov     bp, -1
gp_zm2: lodsb
        gp_fifo
        out     GP_GDCS, al
        loop    gp_zm2
        cld
        add     dh, 40h
        cmp     dh, 0C0h
        jb      gp_zm1
        pop     si
        pop     dx
        pop     cx
        add     word ptr ds:[GP_RADR], 40
gp_zm3: dec     word ptr ds:[GP_ZL]
        jns     gp_zm4
        jmp     gp_ende
gp_zm4: mov     di, ds:[GP_RLEN]
        pop     ax
        ret
gp_zeilm ENDP

IFNDEF GP_NURKERN
gp_puf  db      GP_GROESSE dup (?)
ENDIF

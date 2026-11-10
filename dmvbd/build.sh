#!/bin/sh
# Baut DMVBD.EXE (DOS) und die eigenen Module (Win16-DLLs) mit Open Watcom
# (wcc, wlink, wrc) und JWasm.
#   WATCOM  Open-Watcom-Verzeichnis (mit binl64/, h/, lib286/)
#   JWASMEXE Aufruf von JWasm (Vorgabe: jwasm im PATH)
# Code: Claude, Prompts: rfka01
set -e
: ${WATCOM:?WATCOM auf das Open-Watcom-Verzeichnis setzen}
: ${JWASMEXE:=jwasm}
unset JWASM                     # JWasm liest JWASM als Optionen
export WATCOM PATH=$WATCOM/binl64:$PATH
D=$(cd $(dirname $0); pwd); O=$D/build; mkdir -p $O; cd $O
for f in BD NELOAD W16 W16TAB DRV7220; do
  wcc -zq -ms -bt=dos -0 -os -oi -s -w1 -I$WATCOM/h -fo=$f.obj $D/src/$f.C
done
(cd $D/src && $JWASMEXE -omf -q -Fo$O/BLT86.obj BLT86.ASM)
(cd $D/src && $JWASMEXE -Zm -omf -q -Fo$O/GPICW.obj GPICW.ASM)
wlink op q system dos op map=DMVBD.MAP name DMVBD.EXE file BD.obj,NELOAD.obj,W16.obj,W16TAB.obj,DRV7220.obj,BLT86.obj,GPICW.obj
# Module (Win16-DLLs): Verzeichnis:Name
for m in flydmv:FLYDMV pickover:PICKOVER spiro:SPIRO dmvwarp:DMVWARP; do
  dir=${m%%:*}; n=${m##*:}
  wcc -zq -ms -bt=windows -bd -zu -zw -0 -os -s -w1 -I$WATCOM/h -I$WATCOM/h/win -fo=$n.obj $D/$dir/$n.C
  (cd $D/$dir && wrc -q -r -bt=windows -I$WATCOM/h/win -fo=$O/$n.res $n.RC)
  cat > $n.LNK <<LNK
system windows dll initinstance
option quiet, map=$n.MAP, heapsize=1024
option description '$n - Code: Claude, Prompts: rfka01'
option resource=$n.res
name $n.AD
file $n.obj
libfile $WATCOM/lib286/win/libentry.obj
export Module.1 resident
export WEP.2 resident
LNK
  wlink @$n.LNK
done
ls -l DMVBD.EXE *.AD

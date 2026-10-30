# Mahjongg - Simulation am PC (gcc) mit DMVGRAF aus rfka01/dmv_goodies.
# DMVGRAF = Pfad zu dmv_goodies/dmvgraf/src
DMVGRAF ?= ../../dmv_goodies/dmvgraf/src
CC      = gcc
CFLAGS  = -x c -DHOST -std=gnu89 -O1 -g -w -I$(DMVGRAF) -Isrc
LIB     = $(DMVGRAF)/GRAF.C $(DMVGRAF)/GBILD.C $(DMVGRAF)/GEIN.C \
          $(DMVGRAF)/GKERN.C $(DMVGRAF)/GHWSIM.C

mjsim: src/mahjongg.c src/mj.h
	$(CC) $(CFLAGS) -o $@ src/mahjongg.c $(LIB)

DMVSTEIN.TIL: tools/mktil.py tools/tiles_def.py
	python3 tools/mktil.py $@

clean:
	rm -f mjsim *.ppm

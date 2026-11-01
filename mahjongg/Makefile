# Mahjongg - Simulation am PC (gcc) und CP/M-68K-Fassung mit DMVGRAF aus
# rfka01/dmv_goodies.
# DMVGRAF = Pfad zu dmv_goodies/dmvgraf/src
DMVGRAF ?= ../../dmv_goodies/dmvgraf/src
CC      = gcc
CFLAGS  = -x c -DHOST -std=gnu89 -O1 -g -w -I$(DMVGRAF) -Isrc
LIB     = $(DMVGRAF)/GRAF.C $(DMVGRAF)/GBILD.C $(DMVGRAF)/GEIN.C \
          $(DMVGRAF)/GKERN.C $(DMVGRAF)/GHWSIM.C

mjsim: src/mahjongg.c src/mj.h
	$(CC) $(CFLAGS) -o $@ src/mahjongg.c $(LIB)

# CP/M-68K (K234), mit gcc fuer den 68000 (DMVGRAF tools/bau68.py)
G68 = $(DMVGRAF)/GCRT68.S $(DMVGRAF)/GSUP68.S $(DMVGRAF)/GARG68.C \
      src/mahjongg.c $(DMVGRAF)/GRAF.C $(DMVGRAF)/GBILD.C $(DMVGRAF)/GEIN.C \
      $(DMVGRAF)/GK68.S $(DMVGRAF)/GHWCPM.C $(DMVGRAF)/GHD68K.C \
      $(DMVGRAF)/GHPOOL.C

MAHJONGG.68K: src/mahjongg.c src/mj.h
	python3 $(DMVGRAF)/../tools/bau68.py -I $(DMVGRAF) -I src \
		-D GBPOOL=61440 -o $@ $(G68)

# MS-DOS (8088/V20) mit gcc-ia16 und DMVGRAF aus lib/dos
MAHJONGG.EXE: src/mahjongg.c src/mj.h
	python3 $(DMVGRAF)/../tools/baudos.py -I $(DMVGRAF) -I src -o $@ \
		$(DMVGRAF)/../lib/dos/GCRTDOS.O src/mahjongg.c \
		$(DMVGRAF)/../lib/dos/DMVGRAF.A

DMVSTEIN.TIL: tools/mktil.py tools/tiles_def.py
	python3 tools/mktil.py $@

clean:
	rm -f mjsim MAHJONGG.68K MAHJONGG.EXE *.ppm

import sys; sys.path.insert(0,'tools')
from sprites_def import *
from PIL import Image
RGB={'K':(0,0,0),'B':(0,0,255),'G':(0,255,0),'C':(0,255,255),'R':(255,0,0),'M':(255,0,255),'Y':(255,255,0),'W':(255,255,255)}
def pad_bg(w,h):
    im=Image.new('RGB',(w,h))
    for y in range(h):
        for x in range(w):
            im.putpixel((x,y),(255,255,0) if (x+y)&1==0 else (0,0,0))
    return im
items=[('LF',LIB_F,1),('LB',LIB_B,1),('LL',LIB_L,1),('LR',LIB_R,1),('VF',DUMMVOGEL,1),('VG',VOGEL_GEB,1),('VL',VOGEL_L,1),('VR',VOGEL_R,1),
       ('Kaefer',KAEFER,0),('Kroete',KROETE,0),('Biene',BIENE,0),('Bluete',BLUETE,0)]
S=4
sheet=Image.new('RGB',(6*40*S,2*40*S),(0,0,160))
for i,(n,rows,z) in enumerate(items):
    bg=pad_bg(40,40)
    k=z+1
    for y,row in enumerate(rows):
        assert len(row)==16,(n,y,len(row))
        for x,c in enumerate(row):
            if c.islower(): c = c.upper() if (x+y)%2==0 else 'K'
            if c in RGB:
                for dy in range(k):
                    for dx in range(k):
                        bg.putpixel((20-8*k+x*k+dx,20-8*k+y*k+dy),RGB[c])
    sheet.paste(bg.resize((40*S,40*S),Image.NEAREST),((i%6)*40*S,(i//6)*40*S))
sheet.save(sys.argv[1])

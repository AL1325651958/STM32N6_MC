"""Author original MiniCraft pixel sprites; generate a packed ROM atlas and PNG sheet.

Run with Python + Pillow. No downloaded artwork, runtime image decoder or SD access.
Each 16x16 icon has transparency plus 15 RGB565 colors (160 ROM bytes/item).
"""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
NAMES = ['AIR', 'GRASS', 'DIRT', 'STONE', 'SAND', 'LOG', 'LEAVES',
         'BEDROCK', 'WATER', 'COBBLE', 'PLANKS', 'WORKBENCH', 'FURNACE',
         'COAL ORE', 'IRON ORE', 'GLASS', 'BRICKS', 'TORCH', 'STICKS', 'COAL',
         'IRON', 'WOOD PICK', 'STONE PICK', 'IRON PICK', 'APPLE', 'AXE',
         'SHOVEL', 'SWORD', 'CHEST', 'BED', 'SAPLING', 'DOOR', 'DOOR OPEN']
NAMES += ['STONE SLAB', 'SLAB TOP', 'FARMLAND', 'WHEAT CROP', 'WHEAT MATURE', 'WHEAT SEEDS', 'WHEAT']
NAMES += ['FENCE', 'STONE STAIRS', 'STONE STAIRS EAST', 'STONE STAIRS SOUTH', 'STONE STAIRS WEST']
NAMES += ['RAW MEAT']
NAMES += ['REDSTONE DUST', 'REDSTONE DUST ON', 'REDSTONE LAMP', 'REDSTONE LAMP ON', 'LEVER', 'LEVER ON', 'BUTTON', 'BUTTON ON']
NAMES += ['BOW', 'ARROW']
BASE = {1:(112,165,55), 2:(139,96,61), 3:(141,145,151), 4:(224,208,150),
        5:(144,100,53), 6:(62,135,49), 7:(67,69,77), 8:(55,135,207),
        9:(133,139,146), 10:(191,145,83), 11:(169,116,59), 12:(115,121,128),
        13:(143,148,153), 14:(143,148,153), 15:(145,219,231),
        16:(183,85,62), 28:(177,122,51)}


def shade(c, n):
    return tuple(max(0, min(255, v+n)) for v in c)


def grain(u, v, seed):
    return ((u*37 ^ v*73 ^ (u*v+seed)*19) & 15)-7


def tex(item, u, v, face):
    c = BASE[item]
    n = grain(u, v, item)*2
    if item == 1:
        c = BASE[1] if face == 0 or v < 4+(u % 3) else BASE[2]
    elif item == 5:
        if face == 0:
            c = (193,155,95)
            if max(abs(u-7), abs(v-7)) % 3 == 0:
                n -= 38
        elif (u+(v//5)) % 4 == 0:
            n -= 38
    elif item == 6:
        n *= 2
    elif item == 7:
        n *= 3
    elif item == 8:
        n = 24 if (u+v//3) % 7 == 0 else -5
    elif item == 9:
        if v % 5 == 0 or (u+(v//5 % 2)*4) % 7 == 0:
            n -= 48
    elif item == 10:
        if v % 4 == 0 or (u+v//4*7) % 16 == 0:
            n -= 45
    elif item == 11:
        if face == 0:
            c = (192,149,87)
            if u % 5 == 0 or v % 5 == 0:
                n -= 70
        elif u < 2 or u > 12 or v > 12:
            n -= 55
        elif face == 1 and 4 < u < 10 and 4 < v < 11:
            c = (76,53,33)
    elif item == 12:
        if face == 1 and 3 < u < 12 and 4 < v < 12:
            c = (33,35,42)
            n = 0
        elif v % 5 == 0 or u % 6 == 0:
            n -= 24
    elif item in (13, 14):
        if grain(u//3, v//3, item) < -1:
            c = (43,44,49) if item == 13 else (207,160,127)
            n = grain(u, v, item)
    elif item == 15:
        if u < 2 or v < 2 or u > 13 or v > 13:
            c = (191,241,245)
        elif u == v or u == v+1:
            c = (235,252,250)
        else:
            return None
    elif item == 16:
        if v % 4 == 0 or (u+(v//4 % 2)*4) % 8 == 0:
            c = (197,186,165)
            n = 0
    elif item == 28:
        if v < 2 or v > 13 or u < 2 or u > 13 or v == 6:
            c = (91,57,30)
        elif face == 1 and 6 <= u <= 9 and 5 <= v <= 10:
            c = (246,209,91)
            n = 5
        elif u % 5 == 0:
            n -= 18
    return shade(c, n + (12 if face == 0 else -10 if face == 1 else -39))


def cube(item):
    im = Image.new('RGBA', (16,16))
    d = ImageDraw.Draw(im)
    d.polygon([(8,0),(15,4),(15,11),(8,15),(1,11),(1,4)], fill=(29,32,35,255))
    faces = [[(8,1),(14,4),(8,7),(2,4)],
             [(2,5),(7,8),(7,13),(2,10)],
             [(9,8),(14,5),(14,10),(9,13),(8,14),(8,9)]]
    for f, poly in enumerate(faces):
        mask = Image.new('1', (16,16))
        ImageDraw.Draw(mask).polygon(poly, fill=1)
        for y in range(16):
            for x in range(16):
                if not mask.getpixel((x,y)):
                    continue
                if f == 0:
                    u = round(((y-1)/3-(x-8)/6)*7)
                    v = round(((y-1)/3+(x-8)/6)*7)
                else:
                    u = round(((x-2)/6 if f == 1 else (14-x)/6)*15)
                    v = round((y-4-((x-2) if f == 1 else (14-x))*.5)/7*15)
                u, v = u & 15, v & 15
                c = tex(item, u, v, f)
                if c is None or (item == 6 and grain(u,v,31) < -5):
                    im.putpixel((x,y), (0,0,0,0))
                else:
                    im.putpixel((x,y), (*c,255))
    return im


def sprite(item):
    if item in BASE:
        return cube(item)
    im = Image.new('RGBA', (16,16))
    d = ImageDraw.Draw(im)
    edge = '#252931'
    if item == 17:  # Torch: distinct flame, charred collar and wooden shaft.
        d.polygon([(7,15),(4,14),(7,5),(11,6)], fill=edge)
        d.line([(6,13),(9,6)], fill='#a06b32', width=3)
        d.line([(6,13),(9,6)], fill='#d49a4a', width=1)
        d.polygon([(5,6),(5,3),(7,4),(8,0),(10,3),(11,2),(12,6),(10,9),(7,8)], fill='#a94a20')
        d.polygon([(6,5),(8,2),(9,4),(11,4),(11,6),(9,8),(7,7)], fill='#ff9d22')
        d.polygon([(7,6),(9,3),(10,6),(9,7)], fill='#fff19b')
    elif item == 18:
        d.line([(3,13),(12,3)], fill=edge, width=4)
        d.line([(3,12),(11,3)], fill='#a9773e', width=3)
        d.line([(3,12),(11,3)], fill='#e0ad63', width=1)
    elif item == 19:
        d.polygon([(2,8),(5,3),(10,2),(14,6),(13,12),(7,14),(2,12)], fill=edge)
        d.polygon([(3,8),(6,4),(10,3),(13,6),(11,10),(5,11)], fill='#4c5261')
        d.polygon([(5,11),(11,10),(13,7),(12,11),(7,13),(3,11)], fill='#343843')
        d.line([(5,7),(7,5),(9,5)], fill='#747b86', width=1)
    elif item == 20:
        d.polygon([(1,7),(5,3),(12,3),(15,7),(12,12),(4,13),(1,10)], fill=edge)
        d.polygon([(2,7),(6,4),(11,4),(14,7),(10,9),(4,9)], fill='#f1f1e1')
        d.polygon([(2,8),(4,10),(10,10),(10,12),(4,12),(2,10)], fill='#9ba7b2')
        d.polygon([(11,10),(14,8),(12,11),(11,12)], fill='#697f94')
        d.line([(6,5),(11,5)], fill='#ffffff')
    elif item in (21,22,23,25,26,27):
        d.line([(3,13),(10,6)], fill=edge, width=4)
        d.line([(3,13),(10,6)], fill='#a7763c', width=2)
        d.line([(3,12),(9,6)], fill='#dca761', width=1)
        material = '#c89b58' if item in (21,25,26) else '#a6b0bb' if item == 22 else '#e0edee'
        dark = '#80552d' if item in (21,25,26) else '#606b7b' if item == 22 else '#80999f'
        if item in (21,22,23):
            d.polygon([(3,1),(9,1),(14,5),(14,9),(12,8),(11,5),(7,4),(3,4),(1,6),(1,3)], fill=edge)
            d.polygon([(3,2),(9,2),(13,5),(13,7),(11,4),(7,3),(3,3),(2,4)], fill=material)
            d.line([(4,4),(8,4),(11,6)], fill=dark)
        elif item == 25:
            d.polygon([(7,1),(12,1),(14,3),(14,6),(10,9),(7,7),(8,4)], fill=edge)
            d.polygon([(8,2),(12,2),(13,4),(13,6),(10,8),(8,6),(9,4)], fill=material)
            d.line([(9,6),(11,6),(13,4)], fill=dark)
        elif item == 26:
            d.polygon([(10,1),(14,2),(15,5),(12,9),(8,7),(7,4)], fill=edge)
            d.polygon([(10,2),(13,3),(14,5),(12,7),(9,6),(8,4)], fill=material)
            d.line([(9,5),(11,6),(13,4)], fill=dark)
        else:
            d.polygon([(5,10),(9,4),(14,0),(15,1),(13,6),(8,12)], fill=edge)
            d.polygon([(7,9),(11,4),(14,1),(13,5),(8,10)], fill=material)
            d.line([(8,9),(13,4)], fill=dark)
            d.line([(4,8),(9,13)], fill=edge, width=3)
            d.line([(4,8),(9,13)], fill='#cfab5b', width=1)
    elif item == 24:
        d.polygon([(7,5),(8,1),(9,1),(8,5)], fill='#775431')
        d.polygon([(9,3),(11,1),(14,2),(12,4)], fill='#5c9f43')
        d.polygon([(2,6),(4,4),(7,4),(8,5),(10,4),(12,4),(14,7),(13,12),(10,15),(7,14),(5,15),(2,12),(1,8)], fill=edge)
        d.polygon([(3,6),(5,5),(7,6),(9,6),(11,5),(13,7),(12,12),(10,14),(7,13),(5,14),(3,11),(2,8)], fill='#c6383a')
        d.polygon([(3,7),(5,6),(6,7),(5,10),(3,10)], fill='#ff9b76')
        d.line([(12,9),(11,12),(9,13)], fill='#8f2533')
    elif item == 45:
        # Raw meat: a compact pink-red cut with a pale fat edge and bone.
        d.polygon([(1,8),(4,4),(10,3),(15,6),(14,11),(10,14),(4,13),(1,10)], fill=edge)
        d.polygon([(3,8),(5,5),(10,4),(13,6),(13,10),(9,12),(5,11),(3,10)], fill='#c94e58')
        d.polygon([(4,7),(7,5),(10,5),(12,7),(10,8),(7,7),(5,9)], fill='#ef8c7f')
        d.polygon([(10,9),(13,7),(13,10),(10,12)], fill='#a93645')
        d.ellipse((2,9,5,12), fill='#f1d7bd', outline=edge)
    elif item in (46,47):
        # Redstone dust: a small cross of powder, with a brighter powered state.
        c = '#f12b36' if item == 47 else '#9f1f2b'
        d.line([(2,8),(13,8)], fill=edge, width=4);d.line([(8,3),(8,13)], fill=edge, width=4)
        d.line([(2,8),(13,8)], fill=c, width=2);d.line([(8,3),(8,13)], fill=c, width=2)
        for x,y in [(4,5),(11,11),(5,11),(11,5)]: d.rectangle((x,y,x+1,y+1), fill='#ff7a55' if item==47 else '#ce3540')
    elif item in (48,49):
        # Lamp block, with the powered variant glowing warm yellow.
        fill='#f4c43c' if item==49 else '#6e6257'
        d.rectangle((1,1,14,14), fill=edge);d.rectangle((3,3,12,12), fill=fill)
        d.rectangle((5,5,10,10), fill='#fff0a0' if item==49 else '#a79b80')
        d.line([(3,4),(12,4)], fill='#ffe67a' if item==49 else '#81786a')
    elif item in (50,51):
        # Lever and its powered state; the state is an internal map shape but
        # shares the same silhouette for inventory previews.
        d.rectangle((2,10,13,13), fill=edge);d.rectangle((3,10,12,11), fill='#9c6b3b')
        d.line([(8,10),(11 if item==51 else 5,3)], fill=edge, width=4)
        d.line([(8,10),(11 if item==51 else 5,3)], fill='#e7b55a' if item==51 else '#a9773e', width=2)
    elif item in (52,53):
        d.rectangle((2,5,13,12), fill=edge);d.rectangle((4,6,11,10), fill='#d69a55' if item==53 else '#a9773e')
        d.rectangle((5,7,10,9), fill='#f3c26a' if item==53 else '#c08b4d')
    elif item == 54:
        # Bow: a curved wooden limb, taut string, and a small grip.
        d.arc((1,1,14,15), 72, 288, fill=edge, width=4)
        d.arc((2,2,13,14), 72, 288, fill='#b9783e', width=2)
        d.line([(8,3),(8,13)], fill='#ead8a5', width=1)
        d.rectangle((6,7,10,10), fill='#77482d')
        d.line([(7,7),(9,10)], fill='#c18a4c', width=1)
    elif item == 55:
        # Arrow: iron head, wooden shaft, and two contrasting fletchings.
        d.line([(3,13),(13,3)], fill=edge, width=3)
        d.line([(3,13),(12,4)], fill='#d0a064', width=1)
        d.polygon([(12,1),(15,3),(12,6),(11,4)], fill=edge)
        d.polygon([(12,2),(14,3),(12,5)], fill='#dce5e7')
        d.polygon([(3,13),(1,10),(5,11),(5,14)], fill='#6c9c62')
    elif item == 29:
        d.polygon([(1,8),(9,3),(15,6),(15,11),(7,15),(1,12)], fill=edge)
        d.polygon([(2,8),(9,4),(14,6),(7,11)], fill='#e3544b')
        d.polygon([(8,5),(10,4),(13,6),(11,8)], fill='#f9f0cd')
        d.polygon([(2,9),(7,12),(7,14),(2,11)], fill='#b22f38')
        d.polygon([(8,12),(14,8),(14,10),(8,14)], fill='#803338')
        d.point((2,12), fill='#b58a4d');d.point((8,15), fill='#b58a4d');d.point((14,11), fill='#b58a4d')
    elif item == 30:
        d.line([(8,14),(8,5)], fill='#573f2a', width=3)
        d.line([(8,13),(8,5)], fill='#af8047')
        d.line([(7,9),(4,6)], fill='#916b36');d.line([(9,8),(12,5)], fill='#916b36')
        for x,y in [(3,4),(10,3),(6,1),(3,8),(10,7)]:
            d.polygon([(x,y+1),(x+2,y),(x+4,y+1),(x+3,y+4),(x,y+3)], fill='#25452b')
            d.polygon([(x,y+1),(x+2,y),(x+3,y+1),(x+2,y+3),(x,y+2)], fill='#68ac49')
            d.point((x+1,y+1), fill='#afce62')
        d.line([(5,14),(10,14)], fill='#795530')
    elif item == 38:
        # Wheat seeds: a small warm seed cluster with a dark outline.
        for x,y in [(4,11),(7,8),(10,11),(8,5),(11,6)]:
            d.line([(x-1,y+2),(x+1,y-1)], fill=edge, width=3)
            d.point((x,y), fill='#e2bd58')
    elif item == 39:
        # Wheat: mature golden ears and a green stem.
        d.line([(7,15),(8,5)], fill='#5c8d38', width=2)
        d.line([(8,12),(4,8)], fill='#75a441', width=2)
        d.line([(8,10),(12,6)], fill='#75a441', width=2)
        for x,y in [(5,7),(4,5),(10,5),(12,4),(7,3),(9,1)]:
            d.polygon([(x,y+1),(x+2,y),(x+3,y+1),(x+1,y+3)], fill='#e3bd55')
    elif item in (31, 32):
        # A compact wooden door icon; the open variant shows the rotated slab.
        if item == 31:
            d.rectangle((3,1,12,14), fill=edge)
            d.rectangle((5,2,10,14), fill='#9d6935')
            d.line((7,3,7,13), fill='#d29a4c', width=1)
            d.line((9,3,9,13), fill='#704522', width=1)
            d.rectangle((6,5,9,9), outline='#704522', width=1)
            d.rectangle((10,8,11,9), fill='#f0c568')
            d.line((4,1,11,1), fill='#d7a55d', width=1)
        else:
            d.polygon([(2,3),(6,1),(9,2),(5,5),(5,14),(2,13)], fill=edge)
            d.polygon([(3,3),(6,2),(8,3),(4,5),(4,13),(3,12)], fill='#a8733a')
            d.line([(4,6),(4,12)], fill='#d29a4c', width=1)
            d.point((6,6), fill='#f0c568')
    elif item in (33, 34):
        # The top state is an internal map shape but shares the held-item icon.
        d.polygon([(2,8),(8,4),(14,8),(8,12)], fill=edge)
        d.polygon([(3,8),(8,5),(13,8),(8,10)], fill='#aeb4b9')
        d.polygon([(3,8),(8,10),(8,14),(3,11)], fill='#747b83')
        d.polygon([(8,10),(13,8),(13,11),(8,14)], fill='#5d636b')
        d.line([(4,8),(8,6),(12,8)], fill='#e0e4e4')
    elif item == 40:
        d.rectangle((6,1,9,14), fill=edge)
        d.rectangle((7,2,8,13), fill='#a9773e')
        d.rectangle((1,5,14,7), fill=edge);d.rectangle((2,5,13,6), fill='#b77d42')
        d.rectangle((1,10,14,12), fill=edge);d.rectangle((2,10,13,11), fill='#a36c37')
    elif item in (41,42,43,44):
        # The orientation variants intentionally reuse the same item silhouette.
        d.polygon([(1,10),(8,5),(15,9),(8,14)], fill=edge)
        d.polygon([(2,9),(8,5),(14,9),(8,11)], fill='#b4bac0')
        d.polygon([(2,9),(8,11),(8,14),(2,11)], fill='#7b838c')
        d.polygon([(8,11),(14,9),(14,12),(8,14)], fill='#626a73')
        d.line([(4,9),(8,7),(12,9)], fill='#e7ebeb', width=1)
    return im


def rgb565(c):
    return (c[0] >> 3) << 11 | (c[1] >> 2) << 5 | (c[2] >> 3)


def packed(im):
    # Quantize only opaque pixels; transparent black must not waste a color.
    source = [im.getpixel((x,y)) for y in range(16) for x in range(16)]
    opaque = [p[:3] for p in source if p[3]] or [(0,0,0)]
    swatch = Image.new('RGB', (len(opaque),1));swatch.putdata(opaque)
    q = swatch.quantize(colors=15, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    palette = q.getpalette()
    triples = [palette[i:i+3] for i in range(0,len(palette),3)][:15]
    indices = [1+min(range(len(triples)),key=lambda i:sum((p[k]-triples[i][k])**2 for k in range(3))) if p[3] else 0 for p in source]
    palette += [0]*(45-len(palette))
    colors = [0]+[rgb565(palette[i:i+3]) for i in range(0,45,3)]
    pixels = [(indices[i] << 4) | indices[i+1] for i in range(0,256,2)]
    decoded = Image.new('RGBA', (16,16))
    decoded.putdata([((colors[i]>>11)*255//31,((colors[i]>>5)&63)*255//63,(colors[i]&31)*255//31,255) if i else (0,0,0,0) for i in indices])
    return colors, pixels, decoded


def main():
    asset = ROOT/'assets/minicraft';asset.mkdir(parents=True, exist_ok=True)
    entries = [packed(sprite(i)) for i in range(len(NAMES))]
    header = ['/* Generated by tools/make_minicraft_icons.py; original project artwork. */',
              '#ifndef MC_ICONS_H', '#define MC_ICONS_H',
              '#define MC_ICON_EDGE 16',
              'typedef struct { uint16_t colors[16]; uint8_t pixels[128]; } MC_Icon;',
              'static const MC_Icon mc_icons[MC_ITEM_COUNT] = {']
    atlas = Image.new('RGBA', (16*len(NAMES),16))
    sheet = Image.new('RGB', (800,4*112), '#121d29');d=ImageDraw.Draw(sheet)
    for i,(colors,pixels,im) in enumerate(entries):
        header += ['  { /* %d %s */'%(i,NAMES[i]),
                   '    {'+', '.join('0x%04x'%v for v in colors)+'},',
                   '    {']
        header += ['      '+', '.join('0x%02x'%v for v in pixels[k:k+16])+',' for k in range(0,128,16)]
        header += ['    }', '  },']
        atlas.paste(im, (i*16,0))
        x=(i%8)*100;y=(i//8)*112
        d.rounded_rectangle((x+5,y+4,x+94,y+107),radius=5,fill='#213040',outline='#405263')
        sheet.paste(im.resize((64,64),Image.Resampling.NEAREST),(x+18,y+10),im.resize((64,64),Image.Resampling.NEAREST))
        d.text((x+10,y+80),NAMES[i],fill='#eef2dd')
        d.text((x+10,y+94),'%02d'%i,fill='#95acb6')
    header += ['};','#endif','']
    (ROOT/'Appli/Core/Inc/mc_icons.h').write_text('\n'.join(header),encoding='utf-8')
    atlas.save(asset/'items.png');sheet.save(asset/'items-sheet.png')
    print('Generated %d sprites, %d packed ROM bytes; %s'%(len(NAMES),len(NAMES)*160,asset))


if __name__ == '__main__':
    main()

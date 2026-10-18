# Formaty plików

## Res/*.xbe (archiwa zasobów; nazwa myląca, to nie Xbox)
- `u32 N`, potem N rekordów po 16 B: `u32 id, u32 table_offset, u32 lo, u32 hi`.
- Grupa zawiera elementy o numerach `[lo, hi)`. Tabela pod `table_offset` to `hi-lo` wpisów `u32` z offsetami początków (rosnące).
- Koniec elementu = następny offset; ostatni element kończy się na pierwszym offsecie następnej grupy (lub na końcu pliku).
- Zweryfikowane na: Pal, light, Units, Tiles0, fnts, common, Snd (src/tools/xbe.py).
- Typy grup: 0x44c / 0x454 = obrazy (nagłówek elementu zaczyna się od `00 00 ac e0`), 0x515 = animacje, Snd 0x460 = pliki WAV, Pal 0x44f/0x450/0x452/0x453/0x479 = palety i tablice kolorów.

## Maps/*.map
- `u16 szerokość, u16 wysokość`, potem `szer*wys` wartości `u32` (indeksy kafli).

## Sprite (elementy typu 0x44c i 0x454 w archiwach)
Nagłówek: `u32 magic=0xE0AC0000, u16 w, u16 h, u8 flags(+8), u8 +9, u8 +10, u8 +11, u32 +12, i16 ox, i16 oy (+0x14)`, od +0x18 dane.
Flagi (`Sprite::Draw`, FUN_0047a990):
- `0x01` surowa bitmapa, wiersze po kolei od +0x18 (`0x10` = piksele 16-bit, inaczej 8-bit indeksy palety; `0x04` = wariant cieniowany).
- `0x02` RLE z indeksami palety 8-bit. Od +0x18 tablica wierszy po 8 B: `u32 off_kontrolny, u32 off_pikseli`. Strumień kontrolny: bajty na przemian skip/draw, aż suma >= w. Piksele to kolejne bajty = indeks palety. `0x04`/`0x08` = wersje cieniowane/półprzezroczyste.
- `0x40` RLE z pikselami 16-bit (RGB 565/555): bajt kontrolny `<0x80` skip n, `0x80-0xBF` skip (n&0x3F), `>=0xC0` draw (n&0x3F) pikseli 16-bit.
- Strumień kontrolny może przekroczyć szerokość o 1 (spotkane przy w=288): przycinać do w.
Obserwowane flagi: 1, 5, 6, 10, 14, 17, 64.

## Pal.xbe (palety)
- Grupa 0x44f = tablice RGB555 (256 x u16), 0x450 = RGB565, 0x479 = 32-bit, 0x452/0x453 = tablice 256x256 (mieszanie/półprzezroczystość).
- Element palety = numer palety (indeks od 1). Rozmiar 512 B = jedna paleta, 10240 B = 20 poziomów cieniowania po 512 B (poziom = `param*0x200`), pozostałe rozmiary to warianty.
- Kafle terenu (Tiles0, flaga 1, 48x48) mają naturalne kolory dla palety 4 przy poziomie cieniowania 14 (w repo: src/tools/xbe.py + sprite.py). Palety 5..17 to motywy terenu.

## Do rozpoznania
`.spr .vis .rm .dat .mp .mit .scn`, nagłówek elementu obrazu (0xE0AC0000), format animacji, `Music/music.xbe` (RIFF WAV), `strbase.bds` (tekst), `Keys.ini`.

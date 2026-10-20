# Formaty plików

## Res/*.xbe (archiwa zasobów; nazwa myląca, to nie Xbox)
- `u32 N`, potem N rekordów po 16 B: `u32 id, u32 table_offset, u32 lo, u32 hi`.
- Grupa zawiera elementy o numerach `[lo, hi)`. Tabela pod `table_offset` to `hi-lo` wpisów `u32` z offsetami początków (rosnące).
- Koniec elementu = następny offset; ostatni element kończy się na pierwszym offsecie następnej grupy (lub na końcu pliku).
- Zweryfikowane na: Pal, light, Units, Tiles0, fnts, common, Snd (src/tools/xbe.py).
- Typy grup: 0x44c / 0x454 = obrazy (nagłówek elementu zaczyna się od `00 00 ac e0`), 0x515 = animacje, Snd 0x460 = pliki WAV, Pal 0x44f/0x450/0x452/0x453/0x479 = palety i tablice kolorów.

## Maps/*.map
- `u16 szerokość, u16 wysokość`, potem `szer*wys` wartości `u32`. Dolne 16 bitów = indeks kafla bazowego w `Tiles{0,1}.xbe` (typ 0x44c, 48x48, zakres od 1000); górne 16 bitów = nakładka (indeks z grupy 0x454, np. 0x132d), 0 = brak.
- Teren rysowany z góry: kafle 48x48, paleta 4, poziom cieniowania 14 (src/tools/render_map.py).

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

## Pliki misji (`Maps/<nazwa>.*`, zweryfikowane na 63 mapach)
- `.map`: patrz wyżej. `.dat`: `w*h` bajtów, mapa przejezdności. Zweryfikowane w `FUN_0046bdb0` (WaveSearch): 0 = zablokowane (woda/obrzeża; obie klasy ruchu 0),
  1 = przejezdne tylko dla pierwszej klasy ruchu (w misji 4 pokrywa się z nakładkami drzew 4909-4913), 2 = przejezdne dla obu klas. Trzecia tablica (lot) = zawsze przejezdna.
- `.rm`: `u16 w, u16 h`, potem `w*h` komórek `u16` = dwa bajty: kanał zasobu A (młodszy), kanał B (starszy). Wartość zależy od typu kafla bazowego
  (1000-1099: 65/0, 1100-1199: 0/65, 1200-1299: 26/26, 0 = brak). Odczyt: `FUN_0042acd0` (średnia ważona kanałów po masce 7x7, wagi z rekordu typu +0xf0/+0xf4)
  i `FUN_0042ad90` (sumy w kole r=4 dla AI). Nazwa zasobu kanałów A/B niepotwierdzona.
- `.vis`: POPRAWKA (zweryfikowano w `FUN_00430ae0`, 49 plików): `u16 liczba_stron`, potem dla każdej strony `u16 w, u16 h, w*h bajtów`. Bajt = poziom
  zaciemnienia/mgły danej strony (0 = jasno, mniejszy = jaśniej; 152 = całkiem ciemno/nieodkryte, 96 = poziom nocy). Loader: `src/core/visfile.*`.
- `.spr`: pojedynczy sprite (nagłówek 0xE0AC0000, flaga 1) = podgląd mapy 2 px na kafel (180x120 dla mapy 90x60), indeksy palety.
- `.mit`: tekst odprawy w cp1250: sekcje `~Pamiętnik`, `~Rozkaz`, `~Cele` zakończone linią `~~` (loader: `visfile.hpp` parse_briefing; w kodzie exe nazwa pliku `.mit` nie została znaleziona, wczytanie niepotwierdzone).
- `.scn`: skrypt scenariusza (tekst, CRLF). Bloki `trigger(N)` / `conditions` / `actions` / `end`. Zbadane 63 pliki, 72 wzorce linii:
  warunki: `always`, `localTime|globalTime == N`, `timer(N) == N`, `side(S).units|towns|enemies == N`, `side(S).unitsInRect(x1,y1,x2,y2) > N`,
  `side(S).unitsOfID(UGI_ABDULA) < N`, `side(S).unitsOfUGIInRect(UGI_x,...)`, `side(S).tactic(N).valid|attacked|idle != N`, `side(S).defence(N).valid == N`,
  `side(S).isEnemy|isFriendly(N) != N`; akcje: `trigger(N).enable|disable`, `printString("..")`, `printStringR("..")`, `side(S).victory|defeat`,
  `side(S).addMoney(N)`, `takeMoney(N)`, `setFriendlyTo|setEnemyTo|setNeutralTo(N)`, `cameraActivate(N,N)`, `centerView(x,y)`,
  `side(S).tactic(N).attackTown(N)|giftToSide(S)|setPath(N)|setCycledPath(N)|moveTo(x,y)|joinTactic(N)|kill`, `boat(N).activate`, `johnka(N).enable`,
  `submarine(N).go`, `timer(N).set(N)|start|kill`. `S` to numer strony lub `local`.
- `.mp`: INI (cp1250). Sekcje: `[Globals]` (Version, MapName, MapSize=w,h, Name/Name#, Note#/Noter# + Notes/Notesr (cele misji), StartTime, TileSet, Winds, MaxPlayers,
  AVI, Music, MusicMask, Sides, Towns, Teams, Boats, Johnkas, Submarines, CurrentTeam), `[Side#]` (Name, Race 0..2 = Ruś/Aztekowie/Chiny, Color, Neutral, OnlyComp,
  Difficulty, StartMoney, Start_X/Start_Y, DisabledID=lista typów, Defence#=x,y,r?,.., DefenceCount, Camera#, CamerasCount), `[Town#]` (Side, Race, Type, Tendency,
  BuildSpeed, ScenarioID, X, Y, Units, Unit#), `[Team#]` (Side, ScenarioID, Units, Unit#), `[Boat#]`, `[Johnka#]`, `[Submarine#]`, `[Paths]` (Count, #=n,x,y,...), `[Sounds]`.
  Wiersz `Unit#` = `x, y, typ, zdrowie` (typ = indeks 0..89 tabeli typów obiektów; zdrowie np. 6000 dla piechoty, 17000 dla ciężkich).
- `Res/strbase.bds`: tekstowa baza napisów (`src/tools/strbase.py`): `*STRING { *ID 0xHEX *ORDINAL n *LANGUAGE { *ENGLISH ".." *RUSSIAN ".." } }`, 237 napisów,
  ordinal 1..238; pole `*RUSSIAN` zawiera w rzeczywistości tekst polski (cp1250). Zawiera nazwy budynków (ordinal 169-193), jednostek (194-210) i komunikaty błędów skryptu.

## Animacje (`Res/Units.xbe`, grupa 0x515)
- Element: `char magic[4]="Anim"`, `u32 wersja=3`, potem tablica `u32` indeksów początków sekwencji (stany animacji), liczba użytych klatek; nagłówek ma 0x208 bajtów.
- Po nagłówku tablica klatek po 32 bajty (`u32 sprite_id` (0 = brak; numeracja jak w grupach 0x44c/0x454 tego archiwum), kilka pól `u32`/`i32`
  (opóźnienie, przesunięcie x/y, znaczniki 0/1/-1), w pozycji +0x1c wartość czasu/flagi). Sekwencje: wiele sekwencji po 8 klatek = 8 kierunków.
- W pliku jest 416 slotów, z czego 112 niepustych. Pola klatek: sprawdzone statystycznie, znaczenie pól 1-7 do potwierdzenia przy rysowaniu jednostek.

## Do rozpoznania
Znaczenie pól klatki animacji, wartości `.rm` i `.vis`, `Music/music.xbe` (RIFF WAV), `Keys.ini`, `*.plr` i `single%i.sav`.


## Skrypt scenariusza `.scn` (parser: `src/core/script.*`, 49/49 plików parsuje się)
Wielkość liter bez znaczenia (`localtime`, `jointactic`, `Timer`), białe znaki dowolne, koniec pliku może mieć znak 0x1A. Napisy błędów: `strbase.bds` 72-104 (np. 75 "Bad comparison operation").
```
trigger(N)  conditions <warunek>*  actions <akcja>*  end
warunek: always | localTime|globalTime CMP N | timer(N) CMP N
  | side(S).enemies|units|towns|tactics|money CMP N | side(S).unitsInRect(x1,y1,x2,y2) CMP N
  | side(S).unitsOfUGIInRect(UGI_x,x1,y1,x2,y2) CMP N | side(S).unitsOfId(UGI_x) CMP N
  | side(S).tactic(N).valid|attacked|idle|locked CMP N | side(S).defence(N).valid CMP N
  | side(S).isFriendly|isNeutral|isEnemy(S) CMP N          CMP: == != < <= > >=   S: liczba lub `local`
akcja: trigger(N).enable|disable | boat(N).activate | johnka(N).enable|disable | submarine(N).go
  | timer(N).set(V)|start|stop|kill | centerView(x,y) | printString("EN") | printStringR("RU, cp1251")
  | side(S).victory|defeat|buildCommand(UGI,..)|moveCommand(x,y)|cameraActivate(i,v)|cameraDeactivate(i)
    |setFriendlyTo|setNeutralTo|setEnemyTo(S)|addMoney(N)|takeMoney(N)
  | side(S).tactic(N).attackTown(id)|moveTo(x,y)|joinTactic(N)|setPath|setInvertedPath|setCycledPath|setCycledInvertedPath(N)|lock|unlock|kill|giftToSide(S)
```
`printString` = tekst angielski, `printStringR` = rosyjski (cp1251). Napis nie może zawierać `"`. Nazwy UGI: `src/core/ugi.*` (90 typów). Semantyka wykonania (początkowy stan włączenia
triggerów, jednostki czasu) NIE zweryfikowana. Wyniki `(S)` w sscanf: `%[loca]s` = `local`.

## `.mp` - uzupełnienia (z odczytu `LoadGame.cpp`: `FUN_0042ffb0`, `00430770`, `00430880`, `0042fc90`, `0042f560`...)
Klucze nieczytane przez grę (edytor): `Version, Remark, Notes/Note#/Noter#, Tendency, BuildSpeed, Difficulty, OnlyComp, CurrentTeam, EditorX/Y, EndPosition, [Sounds]`.
Czytane: Globals `Sides, Towns, Teams, Boats, Johnkas, Submarines, Winds` (liczba wiatrów, max 20), `TileSet` (0/1: wybór tablicy światła dnia), `StartTime` (0..7199, tik doby;
tablica kluczy czasu 9 pól: czas, 3x RGB double, poziom ciemności 6..12, współczynnik 1..3; tablice 0x4a8f68 / 0x4a9338), `Music` (jeśli !=0 numer utworu, inaczej `MusicMask` bitmaska), `AVI`, `MaxPlayers` (lobby).
`[Side#]`: `Race, Color, Neutral, StartMoney(def 100), Start_X/Y, Name, DisabledID (lista typów UGI), Defence# = scenarioId,x,y,typ (lub x,y,typ), DefenceCount, Camera# = x,y,z, CamerasCount`.
`[Town#]`: `Side, X, Y, Race, Type` (0 forpost,1 wieś,2 miasto,3 twierdza; typ budynku z tablicy 0x4bc390[Type*3+Race], -1 dla Azteków/Type 3 = fallback kolumny 1), `ScenarioId, Unit#=x,y,typ,hp, Units`.
`[Team#]`: `Side, ScenarioId, Unit#, Units`. `[Boat#]`: `Side,X,Y,Up,ScenarioId` (typ 84). `[Johnka#]`: jak łódź + `Enabled` (typ 88). `[Submarine#]`: `Side,X,Y,ScenarioId`.
`[Paths]`: `Count`, `i = id, x,y, x,y, ...` (id używane w `setPath`). Klucze `.mp` są niewrażliwe na wielkość liter (`ScenarioID` = `ScenarioId`).

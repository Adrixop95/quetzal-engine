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
- UWAGA, poprawka dla flagi `0x40` (zweryfikowana w blitterach `FUN_0047f0a0`/`FUN_0047f270` i obrazem: cursory, napis "Loading", czcionki): to NIE jest kolor 16-bit.
  Bajt kontrolny `<0x80` pomiń n pikseli; `0x80-0xBF` rysuj (n&0x3f) pikseli po 1 bajcie (indeks palety, pełna krycie); `>=0xC0` rysuj (n&0x3f) pikseli po 2 bajty
  (indeks palety, bajt mieszania/alfa). Paleta = pole nagłówka +0xC (id w Pal.xbe). Dekoder: `src/core/sprite_alpha.*` (`decode_alpha_sprite`). `src/core/sprite.cpp`
  dekoduje te sprite'y błędnie (jako kolor 565), co daje "kontury" zamiast obrazu. Poprawny wynik: 42/448/743 sprite'ów z common/fnts/Units.

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


Dowód `.vis` w grze (misja 4, zrzut RT_DUMP_DIR `shot` vs dekoder): minimapa gry (1 px = 1 kafel, region 638,70 120x90) pokrywa się z warstwą strony 0 w 96,3% pikseli
(ciemne = 152, jasne = 0; plamy w lewym górnym i prawym dolnym rogu o tych samych kształtach). Dowód `.dat`: na renderze kafli misji 4 czarne kafle skał pokrywają się z wartością 0, kępy
drzew z wartością 1 (render `src/tools/render_map.py` + nakładka `.dat`). `.spr` ma 120x90 (1 px/kafel, nie 2) i nagłówek palety 15, obraz wymaga poziomu cieniowania > 0 (poziom 0 palety = czerń; poziom 10 daje poprawne kolory).

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


## Pliki użytkownika
**`*.plr`** (profil gracza, 300 B, `FUN_0043e...`/`FUN_0043f...`; loader/writer: `src/core/userfiles.*`): `+0x00 u8 flaga`, `+0x01` nazwa gracza (NUL, reszta bajtów to śmieci ze stosu),
`+0x44 u32 = 1`, `+0x48 u32 = 0`, od `+0x4c` blok 224 B kopiowany z globalnych ustawień (`0x515430`), które oryginał trzyma w rejestrze
(`HKLM\Software\New Media Generation\The Aztec\...`). Pola bloku (offset względem bloku): `+0x08` Acknowledgement, `+0x0c` tryb (0..3, znaczenie niepewne), `+0x94` SFXVolume,
`+0x98` MusicVolume, `+0x9c` SpeechVolume, `+0xa0` Frequency (0x2b11=11025, 0x5622=22050, inna=44100), `+0xa4` DisplayMode (0..2), `+0xa8` ScrollSpeed, `+0xac` GameSpeed,
`+0xb0` Gamma (double), `+0xb8` ViewAll (bajt), `+0xb9`=1, `+0xba` DefaultProvider. Profil to katalog `Saves/<nazwa>/` plus plik `<timeGetTime>.plr` w katalogu gry
(zapis: `CreateFileA("%i.plr")`, wyszukiwanie `*.plr`). Plik `109555.plr` ma pole `+0x44`=15 (starsza wersja/śmieci).
**`Keys.ini`**: linie `Akcja klawisz` (spacje/tabulatory), wielkość liter bez znaczenia, ta sama akcja może mieć kilka klawiszy. Akcje (id): add 2, inv 3, filtertype 4, filtercondition 5,
fastscroll 6, viewall 7, temporary 8, center 9, putontop 12, garrison 13, send 14, friend 15, autorepair 20, repair 21, destroy 22, aggressive 23, defencive 24, normal 25,
directcontrol 26, lastcaller 27, toggleinterface 28, scrollleft/right/up/down 38-41, printscreen 45, cheatwin 52. Klawisze: nazwy DirectInput (`a`-`z`, `f1`-`f12`, `lshift`, `rshift`,
`lcontrol`, `rcontrol`, `lalt`, `ralt`, `space`, `tab`, `enter`, `caps`, `pad0`-`pad9`, `gray+`, `home`, `left`... pełna tabela w `userfiles.cpp`, kody DIK). Gdy pliku brak, gra używa tablicy domyślnej.
**`aztec_settings.ini`**: NIE jest plikiem oryginału, czyta go tylko zrekompilowany runtime (`runtime/settings.c`): `resolution=WxH`, `window_mode`, `smooth`, `video`, `mouse_speed`.
**`Saves/singleN.sav` / `multiN.sav`** (zapis `FUN_004450e0`, odczyt `FUN_004455f0`): `+0 u32`, `+4 u32` (oba 0), `+8 char[256]` ścieżka .mp misji, `+0x108 char[64]` opis wpisany przez gracza,
`+0x148` SYSTEMTIME (8 x u16), od `+0x158` surowy strumień stanu: pozycja widoku (2 x u32), 5 x u32 globalnych (`0x4cc38c/0x4cc240/0x4cc244/0x4cc23c/0x4c5730`), potem sekcje w kolejności:
pula `0x4c1978`, pula `0x5156f8`, liczba stron i obiekty Side (0x69c każdy, metoda wirtualna +4), pula `0x4d9b50`, jednostki `0x4cc3d0`, `0x515a98`, `0x4c1298`, `0x4c4ce0`, `0x4cd698`, `0x4d9b88`;
każdy obiekt serializuje własna metoda wirtualna. Zdekodowano tylko nagłówek (`parse_save_header`). Dwa zapisy misji 4 zrobione w odstępie ok. 25 s (skrypt RT_SCRIPT: Menu (626,10) -> "Zapisz grę" (250,190) -> wybór slotu
(y = 138+20*slot) -> Zapisz (184,359); slot 1 = `single1.sav`): rozmiar 149924 vs 152428 B (przybywają obiekty), ścieżka w nagłówku `D:\Maps\mission_4.mp`, a pierwsze `u32` strumienia to:
widok x,y (298,336 / 298,324), potem `0,0,1,4`, potem licznik ticków gry (55 -> 363, różnica 308 = 25 s przy 12 Hz). Reszta (ok. 54 tys. różniących się bajtów) to stan obiektów
(wskaźniki vtable i pól, część to surowe wskaźniki/śmieci), układ pól NIE rozpoznany. Wpisanie nazwy przez `key DIK` po kliknięciu slotu nie zadziałało w moim przebiegu (opis pusty).

## Dźwięk (`src/core/audio.*`)
- `Res/Snd.xbe`: grupa `0x460` elementy 1..2013 = pliki WAV (`id dźwięku`); tylko 367 jest niepustych (puste = ten sam offset co następny); formaty mieszane: 44100/22050 Hz, 8/16 bit, mono/stereo.
  Grupa `0x3e8` elementy 1..432 = zestawy dźwięków: tablica `u32` id WAV (275 wpisów, wszystkie wskazują istniejące WAV); gra losuje (`rand`) jeden z zestawu (`FUN_00401480`,
  `FUN_00401250`: liczba = rozmiar/4). Id zestawu to pole [2] klatki animacji (docs/sprites_mapping.md: dźwięk odtwarzany w pierwszym ticku klatki) i inne wywołania.
  Grupa `0x6` elementy 1202..1731 = teksty (cp1250, NUL) napisów do dźwięków mowy; wyświetlane, gdy dźwięk nie może być odtworzony (`FUN_004016b0`).
- Pozycjonowanie: pan = (x - środek widoku) przycięte do +-4000; tłumienie od odległości od środka widoku: <500 px brak, 500..800 liniowo do -4000, dalej wyciszone; dźwięk pozycyjny gra tylko
  gdy komórka mgły ma wartość >= 0x21 (widoczna dla gracza). Typ 4 = potwierdzenia ("Acknowledgement" w opcjach), typ 0 = globalne UI.
- `Music/music.xbe` i `Music_cd2.xbe`: jedna grupa `0x460` (WAV 44100 Hz stereo 16 bit, 20-32 MB); numery 1..12; elementy o zerowej długości leżą na drugiej płycie.
  Maska ścieżek: bit (n-1) = ścieżka n (`FUN_0043a160` sprawdza, które istnieją). `Music=N` w .mp (N != 0) wybiera dokładnie ścieżkę N; inaczej `MusicMask & dostępne`
  (62463 = ścieżki 1-10 i 13-16). Wybór kolejnej ścieżki (`FUN_0043a2xx`, zweryfikowane w kodzie): kursor `0..31` (start 0); gdy żadna ścieżka nie gra i maska != 0, bierze pierwszy ustawiony bit
  od kursora (cyklicznie), gra ścieżkę `kursor+1` (`FUN_00475010`) i zwiększa kursor. Czyli ścieżki z maski grają po kolei w pętli, bez losowania.

## Interfejs, czcionki, światło, palety
- `Res/light.xbe` grupa `0x455` (1..71): maski światła `u16 w=20, u16 h=20, 400 bajtów` ciemności (0 = jasno, max 152 = pełna ciemność). Elementy 1..16 mają rosnącą moc (max 10..152),
  17..63 puste, 64..71 maksimum 152. `FUN_0047d070` nakłada maskę na warstwę mgły strony operacją min (z pozycji w kafelkach). Wartość 152 = 19*8, więc poziom cieniowania palety
  to funkcja `bajt_mgły >> 3` (0..19 = 20 poziomów Pal.xbe), ale kierunek jest odwrócony: poziom 0 palety = czerń, wyższy = jaśniej (kafle naturalne ok. 14), więc jasność ~ `19 - (bajt>>3)`; niepotwierdzone zrzutem.
- `Res/fnts.xbe`: 5 czcionek co 300 id (`FUN_00463...`, `FUN_0047d7f0`): baza 0 (w kodzie 1), 300 (tylko cyfry 48-57), 600, 900, 1200; glif znaku `c` (bajt cp1250) = `baza + c`.
  Wszystkie używają palety 53. Czcionki 600/900 (flaga 5, surowe 8-bit, indeks 0 = tło) i 1200 (flaga 0x40) potwierdzone obrazem (polskie znaki). Baza 0 (flaga 0x40, małe glify 4..16 px)
  to też poprawna czcionka cp1250 (zweryfikowane siatką wszystkich glifów 33..255: 138=Š, 140=Ś, 163=Ł, 165=Ą...); wcześniejsze "krzaczki" wynikały z błędu mojego testu, nie z formatu.
- `Res/common.xbe` grupa `0x44c`: kursory 0 (miecz, 12 klatek 48x48), 2 (bomba, 12 kl.), 3 (czerwone/białe, 9 kl.), 4 (buława 36x36), 5 (młoty, 12 kl. 40x40), 6-13 (strzałki przewijania
  zablokowane, czerwony X; hotspot z ox/oy, kolejność N,NW,W,SW,S,SE,E,NE), 14-20 (strzałki przewijania). Interfejs: 256 "Start Game", 257 przyciski (stos klatek),
  258-263 łuki zdrowia (zielony/żółty/czerwony, 6 rozmiarów), 264/270/285/290 tła dziennika (800x600), 266 monety, 267 belki góra/dół, 273 mapa, 277-279 ikony (poczta, brak złota, pauza),
  280/284 ekrany tła, 281 napis "Loading...", 282 przycisk Menu, 283 listwy, 289/291 zwoje, 292-298 panele kamienne, 299 ptaszek, 300 stos lampek, 308-317 elipsy zaznaczenia (10 rozmiarów x 4 klatki),
  318 znacznik, 319/322-324 paski, 1000-1002 plansze pergaminu, 1536-1538 paski. Palety: kursory 1, tła 55, napisy 48-50, 56-57. Grupa `0x454` ids 128..135: 8 sprite'ów 48x576 (wersje kolorów drużyn).
- Paleta sprite'a = pole +0xC nagłówka. Kafle: Tiles0 (TileSet=0) paleta 4-7 i 13, Tiles1 (TileSet=1) paleta 8-11 i 13. Jednostki: id<4 = paleta drużyny
  (`id + 0x10 + kolor*4`, docs/sprites_mapping.md), 14, 51, 52, 54. Pal.xbe: ids 1..57, grupy 0x44f/0x450/0x479 (palety), 0x452/0x453 (tablice mieszania).


## Wideo (`Video/*.avi`)
- Wszystkie 11 plików: Indeo Video 4 (`IV41`), 576x420, 12 kl./s, dźwięk PCM 22050 Hz (mono; `win.avi`/`lose.avi` stereo). Runtime odtwarza je przez zewnętrzny proces `ffmpeg`
  (`runtime/video.c`: osobne potoki wideo `-f rawvideo -pix_fmt bgra` i audio `s16le 22050 mono`, tempo z `us_per_frame`, Esc/Spacja/Enter/mysz przerywa).
- Kiedy: `intro.avi` przy starcie gry (`FUN_004????`: katalog z rejestru `Video` + `intro.avi`, `FUN_00418e90`); klucz `AVI=` w `[Globals]` mapy kampanii odtwarzany przy wczytaniu misji
  (katalog wideo + nazwa): mission_1 -> `mission1.avi`, Mission_4 -> `Mission4.avi`, mission_5 -> `mission5.avi`, Mission_10 -> `mission9.avi`, Mission_12 -> `Mission11.avi`,
  Mission_13 -> `Mission12.avi`, Mission_14 -> `Mission13.avi`, pozostałe puste (mapy wieloosobowe i samouczki też puste); po misji `win.avi` (zwycięstwo) lub `lose.avi`;
  po 16. misji kampanii (licznik >= 0x10) `final.avi`. Funkcja `0x4191e0`+okolice na starcie otwiera wszystkie pliki Video (kontrola obecności płyty, XOR 0x55), to nie jest wybór filmu.
  (Wskazanie funkcji intro: wywołanie w 17333 pliku all.c.)

## Kursory i interfejs (wybór sprite'ów z `common.xbe`)
- Wybór kursora: `FUN_00464290` -> id sprite'a z `common.xbe` 0x44c: poza grą 4 (buława), przy oknie modalnym 1 (pióro); przy przewijaniu krawędzią (`DAT_005159e0` = kierunek) tablica `0x4bc904`
  (4,4,4,4,16,20,4,4,14,4,18,4,15,21,17,19) -> 14 N, 15 NW, 16 W, 17 SW, 18 S, 19 SE, 20 E (21 dla NE nie istnieje w pliku); gdy przewijanie zablokowane warianty 6..13. Nad widokiem
  `FUN_00447c30` zwraca klasę 0..4 (0 wróg, 3 nic/własny, 1/2/4 tryby budynku), a tablica `0x4bc944` (0,2,3,4,5,...) zamienia ją na id kursora (0 miecz, 2 bomba, 3, 4 buława, 5 młoty).
  Wartości 128..135 w tej tablicy = animowane kursory z grupy 0x454 (48x576, 12 klatek); przypisanie do trybów nie sprawdzone.
- Tablica rozmiarów obiektów `0x4bc890` (wiersze po 3 `i32`): {elipsa zaznaczenia 308..317, łuk zdrowia 263..258, próg szerokości obiektu 55,67,83,102,125,154,189,231,999}.
- Układ ekranu 800x600 (zrzut misji 4): pasek `Menu` (sprite 282) u góry po prawej od x=598, licznik złota obok; minimapa w ramce kamiennej (292/293), obraz minimapy w prostokącie
  x=638..758, y=70..160 (1 px = 1 kafel); kolumna portretów jednostek x~722..780, y~218..390; panel poleceń na dole (294-297, 254x208/288) od x=546, y=392; okna (zapis gry, odprawy)
  na pergaminie (`1000..1002`, 283). Podpowiedzi (tooltips) nie badane.

## Zapisy: przebieg skryptowy i struktura strumienia
- Zapis przez menu (rekompilowana gra, 800x600): `click 626 10` (Menu), `click 250 190` ("Zapisz grę"), `click 200 <138+20*slot>` (slot 1..9), `click 184 359` ("Zapisz" -> okno "Wpisz nazwę"),
  `key <DIK>`... (np. 30=A), `key 28` (Enter) zapisuje `Saves/single<slot>.sav` (slot 0 to pierwszy wiersz). Wpisywanie działa dopiero PO kliknięciu "Zapisz" (sprawdzone zrzutem: pole pokazuje wpisany tekst).
- Symulacja jest deterministyczna: dwa niezależne przebiegi misji 4 dają zapis o identycznym rozmiarze (149924 B) i tym samym liczniku ticków (55) w tym samym punkcie skryptu.
  Zapisy w tik 55 i 81 (26 ticków później): 149924 vs 150664 B, licznik ticków w `u32` nr 6 strumienia (od `+0x158`), widok x,y = pierwsze dwa `u32`.
- Kolejność sekcji wg `FUN_004455f0` (wczytywanie), każda poprzedzona `u32` liczbą elementów: (1) obiekty puli `0x4c1978` (`u32` znacznik, bit 31 = wariant; ctor ze strumienia `FUN_00418800`),
  (2) pula z `FUN_004597d0` (`u32` znacznik z bitem 31), (3) `u32` liczba stron, potem po kolei obiekty Side (0x69c) z własnym deserializerem (vtable[0]), (4) `u32` liczba, każdy: `u32` id, `u32` rodzaj 0..5
  (0 = `FUN_0044eb40`, 2 = `FUN_00408a70`, 3 = `FUN_0044eb40` + `u32`, 4 = `FUN_0042df90`, 5 = `FUN_0044eb40`) - obiekty puli `0x4d9b50`, (5) `u32` liczba, każdy `u32` id, `u32` rozmiar, `FUN_0043db90` (jednostki),
  (6) opcjonalny blok `FUN_0046f3d0`, (7) pula `0x4c1298`: 7 x `u32` + `double` na element, (8) pula `0x4c4ce0` (`FUN_0042a3b0`), (9) pula `0x4cd698` z rodzajami 0..n (`FUN_00446770` + `u32`...).
  Pola poszczególnych klas NIE są rozpisane (każda klasa ma własny deserializer, ~20 funkcji, np. `0x449e70` dla klasy z vtable `0x4940ec`, `0x44eb40` baza obiektu z 85 odczytami).

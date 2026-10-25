# Ruch, wyszukiwanie ścieżek i kolizje w oryginale (Aztec Empire) oraz plan nowego systemu

Dokument opisuje oryginalny stos ruchu (z dekompilacji `work/decomp/all.c`, dodatkowo dekompilacji wybranych adresów Ghidrą i z logów
z rekompilowanej gry), wyniki eksperymentów ilościowych i projekt zamiennika (`src/engine/sim/path/`).
Oznaczenia: **[zw]** = zweryfikowane eksperymentem lub bezpośrednio w kodzie, **[hip]** = hipoteza / niepotwierdzone.
Tabele maszynowe: `docs/data/movement_*.csv`. Wyniki surowe: `docs/data/movement_experiments.csv`, `docs/data/movement_bench.csv`.

## 1. Mapa przejezdności i warstwy `WaveSearch.cpp`

### 1.1 Dane wejściowe
- `DAT_004c4d0c` = bajty pliku `.dat` misji (`w*h`, 1 bajt na kafel 48 px). Wartości: `0` woda/ściana, `1` przeszkoda przejezdna tylko dla małych
  jednostek (rozproszone pojedyncze kafle na lądzie), `2` otwarty ląd. [zw: tak działa konstrukcja warstw, patrz niżej]
- `DAT_00515a54` = szerokość, `DAT_00515a50` = wysokość mapy (z metod `+4/+8` obiektu mapy `DAT_00515428`).
- `FUN_0046bb70` (init, wywoływana przy wczytaniu mapy, `0x46bb70`): alokuje mapę fali `DAT_00515a8c` (`(w+2)*(h+2)` pól `u16`, wyzerowana)
  i 6 warstw bajtowych `w*h`: `DAT_00515a74/78/7c/80/84/88`, potem woła `FUN_0046bdb0`. Zeruje liczniki limitu zapytań `DAT_00515a58..6c`.

### 1.2 Warstwy (`FUN_0046bdb0`, `0x46bdb0`)
Wartość komórki warstwy: `0` = **nieprzejezdna**, `9` = przejezdna, `4` = przejezdna ale sąsiadująca (8 sąsiadów) z nieprzejezdną lub na brzegu mapy
(wolniejsza w fali, patrz 1.5). Typy:

| warstwa | adres | z `.dat` | klasa `param_7` | znaczenie |
|---|---|---|---|---|
| L78 | `DAT_00515a78` | `dat>=2` -> 9, inaczej 0 | 0 | jednostki **duże** (rycerz, bizon, bałwan, słoń; bit `+0x2c` deskryptora = 0) |
| L74 | `DAT_00515a74` | `dat>=1` -> 9, `0` -> 0 | 1 | jednostki **małe** (piechota, łucznik, niedźwiedź, jaguar...; bajt deskryptora `+0x2c` != 0) |
| L80 | `DAT_00515a80` | L78 z karą przyległości (`9` -> `4` gdy sąsiad == 0) | 2 | jak klasa 0 + zapas na ściany (nie używana przez Marine; `param_7 & ~2` w `0x46c330`) |
| L7C | `DAT_00515a7c` | L74 z karą przyległości | 3 | jak klasa 1 + zapas na ściany |
| L84 | `DAT_00515a84` | stała 9 (cała mapa) | 4, 5 | powietrze: nie używana przez falę (klasy z bitem 4 idą torem prostym, 1.7) |
| L88 | `DAT_00515a88` | L84 z krawędziami = 4 | 6, 7 | j.w. |

`FUN_0046c180(ptr, 0)` sprawdza 8 sąsiadów warstwy pod kątem wartości 0. Test przejezdności pojedynczego kafla `FUN_0046c330(&(x,y), klasa)`
(`0x46c330`): dla klas bez bitu 2 `warstwa[kafel] != 0`; dla klas z bitem 2 `(warstwa[kafel] & 8) >> 3` (tylko wartość 9, czyli bez kafli przyległych).
Funkcje kafla jednostek (`CanEnter`, vtable `+0x94`) wołają `FUN_0046c330(.., (bajt +0x2c) != 0)`, czyli klasę 0 lub 1. Warstwy NIE są
modyfikowane w trakcie gry (jedyne zapisy: `0x46bdb0`) [zw: jedyne odwołanie do `.dat` w całym `all.c`]. Zniszczone/postawione budynki wchodzą wyłącznie przez mapę zajętości.

### 1.3 Mapa zajętości (`Map.cpp`, `DAT_004dd2f8`)
Obiekt klasy `Map` (vtable `0x493288`, konstruktory `0x4319e0` (zer.), `0x431b10` (z pliku), `0x431c10` (resize)): `+4` szerokość (bajt), `+8` wysokość,
`+0xc` dane `u32[w*h]`, `+0x10` tablica wskaźników na wiersze (`rows[y][x]`). Instancja `DAT_004dd2f8` to **mapa zajętości**: `0` = wolny kafel,
w przeciwnym razie **wskaźnik na obiekt** (jednostka/budynek; konstruktor jednostki wpisuje `this`, `0x433820`/`0x44ab90` przenoszą wpis przy kroku). Wartości 1-2
oraz >2 (`FUN_0043eac0` przy kontakcie z komórką `>2` woła metodę vtable `+0x68` sąsiada) [hip: 1/2 to stałe przeszkody; zob. 6.1].
Zajętość jest **kafelkowa**: jednostka zajmuje dokładnie jeden kafel (kafel docelowy kroku, patrz 2.1), kafel źródłowy jest zwalniany w chwili rozpoczęcia kroku.

### 1.4 Parametry `wsr_WaveSearch` (`FUN_0046c470`, `0x46c470`, cdecl, 10 argumentów)
`int WaveSearch(sx, sy, gx, gy, out, fog, cls, flag, goals, radius)`; zwraca liczbę punktów ścieżki (0 = start == cel albo start na liście celów).

| arg | znaczenie |
|---|---|
| `sx,sy` | kafel startu (obcinany do mapy; jeśli komórka startu = 0 szukany jest najbliższy kafel przejezdny `FUN_0046ce80(.., 0xffff)`) |
| `gx,gy` | kafel celu (obcinany do mapy), tryb "pojedynczy cel" gdy `goals == 0` |
| `out` | bufor wyniku: tablica trójek `(x, y, z)` (3 x int32); `z = 3` kafel zwykły (wartość warstwy 9/10), `z = 1` kafel przyległy do ściany (wartość 4) |
| `fog` | wskaźnik na obiekt mapy widoczności strony (`Side+0x690`, bajty z `+0x14`, stride `w+1`); komórki z wartością `< 8` ("niezbadane") **wchodzą do fali jako 10 niezależnie od terenu** (patrz 1.5, 6.2) |
| `cls` | klasa ruchu (1.2). `cls & 4` -> tor prosty bez fali (1.7) |
| `flag` (bajt `param_8`) | `0` = uwzględnij mapę zajętości (`FUN_0046cc40`: każda niezerowa komórka zajętości zeruje komórkę fali, chyba że ma wartość 10); `!= 0` = ignoruj jednostki/budynki, tylko teren |
| `goals` | opcjonalna lista celów (lista węzłowa `x,y` par): wszystkie jej komórki dostają znacznik celu `0xf`; jeśli start leży na liście zwracane jest 0 |
| `radius` | `-1` = nieograniczone; inaczej wokół startu rysowany jest pusty kwadrat o promieniu `radius` kafli z wartością 0, czyli **przeszukiwanie ograniczone** do kwadratu `(2r+1)^2` |

Kolejność kroków: kopiuj warstwę do mapy fali (`FUN_0046cdc0`: `u16`, stride `w+2`, ramka zerowa; `fog<8 -> 10`, inaczej wartość warstwy), opcjonalnie nałóż zajętość,
przytnij współrzędne, napraw start, nałóż kwadrat ograniczenia, ustaw znacznik celu(ów) `0xf`, uruchom falę `FUN_0046d330`, jeśli cel nieosiągalny wybierz
najbliższy odwiedzony kafel (`FUN_0046ce80(.., 0xfff0)`: pierścienie Czebyszewa wokół celu, wybiera najmniejszy numer fali w pierwszym pierścieniu z jakimkolwiek odwiedzonym kaflem),
wyciągnij ścieżkę (`FUN_0046ccb0`). Licznik `DAT_00515a70` zlicza zapytania w ticku.

### 1.5 Fala (`FUN_0046d330` + `FUN_0046d4d0`)
- Komórka fali `u16`: niski nibble = wartość warstwy (0 blok, 4, 9, 10, `0xf` cel), starsze bity = numer fali (`0x20 + 0x10*k`), bit `0x10` = start.
- BFS **od startu** w 8 kierunkach (bez zakazu cięcia rogów), w "rundach"; element kolejki `(x, y, opóźnienie)`; nowo odwiedzona komórka dostaje opóźnienie
  `wartość & 7` (9 -> 1, 10 -> 2, 4 -> 4): komórka przyległa do ściany "kosztuje" 4 rundy, nieznana (mgła) 2, zwykła 1. **Metryka = Czebyszew**
  (przekątna kosztuje tyle co prosty), nie euklidesowa; stąd ścieżki o długości do ~1.2x optimum i "schodkowe" tory.
- Kolejki 2 x 24000 B (2000 elementów frontu); koniec gdy front dotknie komórki z nibblem `0xf` albo front pusty (cel nieosiągalny).
- Wydobycie (`FUN_0046ccb0`): od celu wstecz po sąsiadach z numerem fali o 1 mniejszym; remisy rozstrzyga `FUN_0046d1c0` preferując kontynuację poprzedniego kierunku
  (tablica kierunków `DAT_004bcc28`). Wynik odwrócony (`FUN_004820a0`).
- `FUN_0046d650` (`0x46d650`) to zwykłe `a != b`; `Heapsort.cpp` (`0x473030`) nie bierze udziału w fali (sortowanie w innych modułach).

### 1.6 Limit zapytań na tik (`FUN_0046c410`, `0x46c410`)
Dwa sloty `DAT_00515a68/6c` ("przyznane w tym ticku") i dwie pozycje kolejki oczekujących (`DAT_00515a58/5c` liczniki prób, `DAT_00515a60/64` jednostki). Początek ticku
(`0x4577b0`) przepisuje oczekujących do przyznanych i zeruje licznik `DAT_00515a70`. Efekt: **maks. 2 wyszukiwania ścieżki na tik** (24/s przy 12 Hz); jednostka, która nie
dostała slotu, dostaje kod 6 (powtórz za tik) i zwiększa licznik prób; dwie najbardziej "zagłodzone" mają pierwszeństwo w następnym ticku. [zw: log `RT_PATH_LOG`
pokazuje dokładnie po 2 zapytania w kolejnych tikach przy zatorze]

### 1.7 Tor prosty (`param_7 & 4`)
Dla jednostek latających (metoda vtable `+0xa4` != 0, klasa `| 4`): bez fali; punkty co ~1 px po prostej w stałoprzecinkowym 24.8 (Bresenham-like z `FUN_004047e0/f0`
operacjami na liczbach stałych), `z = 3` przy każdym punkcie, ostatni punkt = cel. Liczba punktów = `max(|dx|,|dy|)` kafli + 1 (krok = 1 kafel w osi głównej).

## 2. Ruch jednostek

### 2.1 Marine (`Marine.cpp`, `0x433190`, vtable `0x4932cc`) [zw]
- Stan `+0x64`: 0 martwy/nieaktywny, 1 spoczynek, 2 marsz, 3 atak, 4 umieranie, 5 animacja spoczynku.
- Pozycja: `+0x6c/+0x70` = x,y w 24.8 (px*256), `+0x24/+0x28` px, `+0x44/+0x48` kafel. Wektor kroku `+0x98/+0x9c` (24.8), prędkość = rekord typu dword 5 (px/tik).
- **Krok kafelkowy** `0x433820` (vtable `+0xcc`, `StepTo(tile)`): cel = środek kafla `(x*48+24, y*48+24)`; kierunek 0..255 z `FUN_00461ac0` (atan2), animacja
  `(kąt+16)>>5` = 8 kierunków; wektor = `normalize(cel - pozycja) * prędkość`; stan := 2; **wpis zajętości kafla źródłowego = 0, kafla docelowego = this** (natychmiast),
  `+0x44/48 := cel`; powiadomienie grupy (vtable grupy `+0x124`).
- `Think` (vtable `+0x60`, `0x433db0`) w stanie 2: co tik `pozycja += wektor`; gdy odległość do celu <= prędkość -> przyciągnij do środka kafla, stan 1 i **poproś grupę o
  następny krok** (vtable grupy `+0x11c`); gdy w pobliżu celu (<= 3 * prędkość) a jest cel ataku - przejście do ataku. Jednostka **nie skręca stopniowo** (zmiana kierunku natychmiastowa,
  prędkość stała, brak przyspieszenia).
- `CanEnter(tile)` (vtable `+0x94`, `0x4347f0`): -1 gdy poza mapą, `zajętość != 0` lub `!FUN_0046c330(tile, klasa)`, inaczej 0.
- Zwracane przez jednostkę flagi: `+0xa0` (vtable) = bajt rekordu `+0x2c` (mały/duży, klasa fali 1/0), `+0xa4` = lot (Marine: 0).
- Prędkości (px/tik, rekord typu dword 5): footman 9, rifleman 6, demolisher 7, bear 5, knight 15, archer 8, mage 5, tuzem 10, jaguar 16, bizon 12, ninja 11, hypno 10, chief 10,
  snowman 4, elefant 4, abdula 10. [zw: footman 9 px/tik zmierzone w logu; reszta z rekordów typów, `docs/type_stats.md`]

### 2.2 Inne klasy
| klasa | krok / ruch | kolizje | uwagi |
|---|---|---|---|
| Spider (kind 1, `0x44a350`) | `0x44ab90`: kierunek kwantyzowany do 8 (`bajt +0x91 & 0xe0`), prędkość = rekord `+0x1c` (24.8), stan 2 | ta sama zajętość kafelkowa, `CanEnter` zawsze klasa 0 | czeka z krokiem gdy `+0x64==2` (zapamiętuje cel `+0x88/+0x8c`) |
| Truck (kind 4, `0x460090`), Headtruck (kind 6, `0x42bda0`) | krok kafelkowy jak Marine (`0x4604a0`, `0x42c430`) | zajętość kafelkowa, klasa 0 (`0x44bc80`) | pojazdy: wolniejsze obroty (animacja 8/16 kier.) [hip] |
| Bird (kind 5), Zeppelin (kind 3) | `0x407820`/`0x4716f0`: cel = środek kafla w 24.8, lot po prostej | **brak zajętości** (`CanEnter` = tylko granice mapy) | tor prosty (1.7) |
| Boat (kind 8), Johnka (kind 10) | `+0xcc` = stub; ruch skryptowy (waypointy z `[Boat#]`/`[Johnka#]`, `Boat.cpp` `0x408770`, `Johnka` `0x42dce0`) | poza falą | nie sterowane przez gracza; **nie sprawdzone dokładnie** [hip] |
| Defence/Cannon (kind 7/9) | nieruchome | zajmują kafle jak budynki | |

## 3. Grupy: `Tactic.cpp` (vtable `0x48fbd4`, 83 metody) [częściowo zw]
Każda jednostka ma właściciela (`+0x30` = obiekt grupy, "Tactic"); gracz też dowodzi grupami: `Side+0x688` = aktualnie zaznaczona grupa. Polecenia
(`PCT_FRAME_*`, `0x4577b0`): `CREATEGROUP` `0x15` (`0x448b40`: zaznaczenie prostokątem `d5..d8` = x1,y1,x2,y2 w pikselach), `SELECTGROUP` `0x16` (klik w punkt `d5,d6`),
`ATTACK` `0x18` (`0x447e00`: **prawy klik** = rozkaz ruchu/ataku: `d5,d6` piksele, `d7` flagi; brak celu pod kursorem -> `grupa->vtable[0xd8](kafel = px/48)`),
`FOLLOWTEAM` `0x24` (`0x448210`), `SELECTBYNO` `0x23` (`0x448f60`). Nie ma osobnego `PCT_FRAME_MOVE` (0x17) w przełączniku ticku [zw: brak `case 0x17`].

### 3.1 Przebieg rozkazu ruchu [zw w logach]
1. `grupa->0xd8` (`0x453360`) czyści listę punktów drogi, dopisuje kafel docelowy, woła `0x453920` (`SetGoal`).
2. `0x453920`: ograniczenie zapytań (1.6; brak slotu -> flaga oczekiwania `+0xb8`, próba w następnym ticku); potem **jedno** `WaveSearch(pozycja grupy, cel, out=bufor grupy, fog=Side+0x690,
   cls = pole grupy +0x50, flag=1 (ignoruj jednostki), goals=0, radius=-1)`. Wynik: ścieżka **grupy** (kotwica formacji).
3. Członkowie nie szukają ścieżek sami. Każdy dąży do `kotwica + przesunięcie_formacji` (`0x455b10`, vtable `+0x44`) małymi krokami kafelkowymi po kierunku (8 kier., `0x453ac0` + `0x453e20`).
   Kotwica przesuwa się po ścieżce grupy tylko, gdy jednostki są dostatecznie blisko (tabela spójności `{4,8,8,6}` kafli wg stanu grupy, `0x4516b0`).
4. Ruch uznany za zakończony, gdy jednostka jest w promieniu 144 px (3 kafli) od celu (`0x450910` oznacza jednostki "dotarły" flagą `+0x5f` i grupę flagą `+0xd9`); dalsze jednostki zatrzymują się
   za dotarłymi (kod 3 w `TryStep`).

### 3.2 Krok jednostki i zator (`0x453ac0`, `0x453b60`, `0x453be0`, `0x453e20`)
- Ścieżka członka to **maks. 8 kroków** zapisanych jako nibble `kierunek | (z==3)<<3` (`FUN_0044ff60`, odczyt `FUN_004518c0`): po 8 krokach musi zaplanować kolejny odcinek.
- `0x453b60` (`FollowPath`): bierze następny kierunek (tablica `DAT_004bc81c`: N, NE, E, SE, S, SW, W, NW), `TryMove`; po wyczerpaniu odcinka zwraca 4 (potrzebne nowe planowanie).
- `0x453be0` (`PlanPath`, plan członka): limit zapytań (kod 6 = czekaj), zwalnia własną komórkę zajętości, woła `WaveSearch(pozycja, cel/lista celów, out=DAT_004d6c68, fog,
  cls = (flaga+0xa0 != 0) | (lot ? 4 : 0), flag=0 (**uwzględnij zajętość**), goals, radius=6)`, odtwarza komórkę, pakuje <= 8 kroków; wynik < 2 -> kod 5 (brak ścieżki).
  W logach: zapytania `rad=6` z `flag=0` przy zatorach, `flag=1,rad=-1` dla ścieżek grupy.
- `0x453e20` (`TryStep(delta)`): kafel docelowy zajęty/zablokowany -> zwraca 1 albo próbuje obrócić kierunek o +-45 stopni (`FUN_00462310`/`FUN_004622e0`: obrót kierunku o 1 pozycję) maks. 2 razy;
  gdy zajmującym jest członek tej samej grupy już w pozycji (flaga `+0x5f`) -> kod 3 (stań); przy kodzie 1/2 grupa przelicza formację (`0x450910`).
- **Nie ma wypychania** (push) jednostek ani wzajemnego omijania w sensie ciągłym; konflikt = czekanie / obrót o 45 stopni / ponowne planowanie ograniczone do kwadratu 13x13.


## 4. Warstwa "jak to działa w skrócie" (model do odtworzenia zachowania)
1. Rozkaz ruchu grupy -> **jedna** ścieżka grupy (fala Czebyszewa po terenie, bez jednostek, z mgłą wymuszającą "przejezdne" na niezbadanych kaflach).
2. Członkowie idą krokami kafelkowymi (8 kierunków, jeden kafel zajęty na raz) za kotwicą formacji; konflikt = obrót o 45 stopni / czekanie / lokalne
   przeplanowanie ograniczone do 13x13 kafli (maks. 2 takie zapytania na tik w całej grze).
3. Koniec ruchu = promień 3 kafli od celu + "dotarli" blokują kolejnych (stąd nieregularne skupisko jednostek wokół celu).
4. Brak: ciągłej unikania kolizji, wypychania, ruchu po ułamkach kafla, hierarchii ścieżek, pamięci powyżej 8 kroków.

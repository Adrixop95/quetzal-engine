# Silnik: stan reguł symulacji (zweryfikowane / pomiar / zastępcze)

Symulacja: `src/engine/sim/game_sim.*` (12 Hz, same liczby całkowite, polecenia z kolejki `commands.hpp`), dane z `docs/data/*.csv` wczytywane w czasie
uruchomienia (`rules.*`). Ruch i pathfinding: `src/engine/sim/path/` (agent ruchu; interfejs wymienny: `GameSim` używa tylko `MoveSystem`).
Legenda: **V** = zweryfikowane w oryginale (kod lub zrzut), **M** = zmierzone na rekompilacji, **P** = zastępcze (wymyślone, do wymiany).

| Reguła | Stan | Źródło / uwagi |
|---|---|---|
| Krok 12 Hz, pozycje 24.8 px, hash stanu po każdym ticku, replay z logu poleceń | V (konstrukcja) | brak porównania z oryginałem; hash silnika != hash rekompilacji |
| Typy poleceń (PCT_FRAME_*) i ich numery | V | `docs/object_types.md`; zaimplementowane: PAUSEON/OFF, CREATEGROUP, ADDTOGROUP, MOVE, ATTACK, BUILD, DESTROYHERE, QUEUEKILL, PAUSEBUILD. Zapis tylko w logu: SELECTGROUP, GARRISON, REPEARMODE, ATTACKMODE, BUILDMODE, SELECTBYNO, FOLLOWTEAM |
| Stop (0x50) i AttackMove (0x51) | P | rozszerzenia silnika, oryginał nie ma takich pakietów |
| Kolejność wykonania poleceń w ticku: strona rosnąco, potem kolejność zgłoszenia | P | oryginał: kolejki stron w `0x4577b0` (kolejność nie sprawdzona) |
| HP maks Marine = desc d4; budynek = desc d1 | V | `desc_marine.csv`, `desc_building.csv` |
| HP maks pozostałych typów | P | najwyższe hp tego typu w misji |
| Prędkość Marine = desc d5 px/tik | V | `desc_marine.csv`; nadpisywana pomiarem gdy jest w `speed_measured.csv` |
| Prędkość zmierzona (wszystkie typy z pomiarem) | M | `speed_measured.csv` (średnia w środku biegu) |
| Prędkość lotników, łodzi, nieznanych typów | P | 8 px/tik; lotnik leci po prostej, łodzie stoją |
| Promień jednostki (koło kolizji) | P | 14 px, duże 20 px |
| Klasa ruchu mała/duża (`.dat` 1 = tylko mała) | V (znaczenie `.dat`), P (przypisanie typów) | duże: ciężarówki, niedźwiedź, słoń |
| Cena i czas budowy | V | `types.csv` = funkcje tooltipu; mnożniki BuildSpeed: Town.BuildSpeed nie jest czytane przez grę, więc K=1 |
| Jednostka czasu "Time" (tiki) | P | zakładam 1 jednostka = 1 tik (footman 100 tików = 8,3 s) |
| Drzewo budowy (kto co może, wymagania) | V | `buildtree.csv`, wpisy "A" (budynek obcej rasy) traktowane jak zwykłe wymaganie |
| Ulepszenia ratusza (pozycje typu budynek z flagą 0) | nie zrobione | pomijane w menu |
| `DisabledID` strony blokuje pozycje menu | V | `docs/formats.md` |
| Obrażenia i odstęp strzałów | M dla par z `duel_damage.csv`, P dla reszty (500 dmg / 12 tików) | |
| Zasięg broni | P | wręcz 56 px, dystans 288 px, wieże 336 px; typy wręcz: lista w `rules.cpp` |
| Zasięg widzenia (zdobywanie celów, mgła) | P | 288 px (6 kafli), budynki 8 kafli |
| Automatyczne atakowanie wrogów w zasięgu wzroku (stojące i AttackMove) | P | najbliższy wróg, remis = niższe id; strony `Neutral` pomijane |
| Śmierć i zwłoki | P | animacja śmierci z `Units.xbe`, zwłoki 480 tików, potem znikają; budynki znikają od razu |
| Mgła wojny: kafle w zasięgu = jasne, wcześniej widziane = 96 | P | wartości 0 / 96 / 152 z `.vis`, aktualizacja co 4 tiki |
| Budowa budynku: stawianie na wolnym polu, postęp = czas budowy, etapy = slot 0 animacji | V (etapy), P (czas i brak budowniczych) | oryginał wymaga ekipy/zasobów? niezbadane |
| Obrys budynku w kaflach | P | desc d4/d5 (px) zaokrąglone w górę, wyśrodkowane na kaflu |
| Trening jednostek: kolejka (max 8), pierwsze w kolejce, odnawianie po anulowaniu 100% | P | miejsce pojawienia: pod budynkiem ratusza |
| Dochód: kopalnia +4 / farma +1 co 12 tików | P | Harvest.cpp nierozpoznany |
| Początkowa gotówka = `StartMoney` strony | V | `.mp` |
| Garnizon, naprawa, tryby ataku, populacja, poziomy trudności, AI (Tactic/Defence/Triggers) | nie zrobione | wrogowie tylko bronią się/atakują w zasięgu wzroku |
| Skrypty `.scn` (cele misji, zwycięstwo) | nie zrobione | |

## Skrypty misji (`.scn`, `sim/script_runner.*`)
| Reguła | Stan | Uwagi |
|---|---|---|
| Wszystkie wyzwalacze sprawdzane co tik, akcje gdy wszystkie warunki prawdziwe; `trigger(N).disable` wyłącza | P | wniosek z użycia (`attacked != -1` + samowyłączenie) |
| Wyzwalacze włączane przez inny wyzwalacz startują wyłączone | P | stan początkowy nieudokumentowany |
| `localTime`/`globalTime` = tiki od startu misji | P | |
| Liczniki: od 0, rosną co tik; `set(V)` ustawia i uruchamia, `stop`, `kill`; `timer(0)` istnieje od startu | P | |
| `side(S).units` = żywe nie-budynki; `towns` = żywe ratusze (typy forpost/wieś/miasto/forteca); `enemies` = jednostki i ratusze wrogich stron | P | |
| `tactic(N)` = żywe obiekty strony z `[Team#] ScenarioID = N`; `valid` 0/1; `idle` = wszyscy bezczynni; `attacked` = id ostatniego napastnika lub -1 | P | |
| `attackTown(id)`, `moveTo(x,y)` = AttackMove całej grupy (kafle -> piksele) | P | |
| `joinTactic`, `giftToSide`, `kill`, `lock/unlock`, `setPath/setCycledPath/...` (kolejne punkty z `[Paths]`) | P | |
| `setFriendlyTo/NeutralTo/EnemyTo` zmienia relację jednostronnie (macierz 8x8) | P | |
| `addMoney/takeMoney`, `printString`, `centerView`, `cameraActivate(i,v)` (skok do `Camera#`), `victory/defeat` | P | koniec misji tylko dla strony lokalnej |
| `boat(N).activate`, `johnka(N).enable`, `submarine(N).go`, `buildCommand`, `moveCommand`, `cameraDeactivate` | nie zrobione | liczone jako `unsupported` |
| Obrony z `[SideN] Defence#` pojawiają się jako obiekty (wieże), `defence(N).valid` = żyje | P | |

## Komputer (AI, `sim/ai.cpp`) - szkielet
| Reguła | Stan | Uwagi |
|---|---|---|
| Strony `OnlyComp=1` (nie lokalne, nie neutralne) sterowane przez AI | P | |
| Dochód pasywny +60 co 300 tików | M | `docs/rules_types.md` (obserwacja na `zeco1`) |
| Zakup jednostki co 56 tików, gdy kolejka pusta, losowa pozycja ze stać (deterministyczny RNG) | M (okres) / P (wybór) | oryginał: 161 za piechura (niewyjaśnione) |
| Limit jednostek AI 80 | P | |
| Obrona: trafiony budynek -> bezczynne jednostki w promieniu 10 kafli idą (AttackMove) do napastnika | P | |
| Ataki fal | skrypt `.scn` (`attackTown`, `moveTo`, ścieżki) | AI sama nie planuje ataków |
| `Tendency`, `BuildSpeed`, `Difficulty` z `.mp` | brak wpływu | gra ich nie czyta / wpływ niezmierzony |

## Dźwięk (`src/engine/audio/`)
| Reguła | Stan | Uwagi |
|---|---|---|
| Efekty z `Snd.xbe`: zestaw dźwięków (`sound_set`) z pola [2] klatki animacji, odtwarzany w pierwszym ticku klatki, losowy element zestawu | V (format), P (losowanie: lokalny LCG, nie RNG symulacji) | |
| Pan = (x - środek widoku) przycięte do +-4000 setnych dB; tłumienie: brak do 500 px, liniowo do -4000 przy 800 px, dalej cisza | V (z docs/formats.md), P (jednostka 100 = 1 dB) | pomijany warunek "komórka mgły >= 0x21" |
| Muzyka: `Music=N` -> ścieżka N, inaczej `MusicMask`; kolejne utwory po zakończeniu | V (wybór maski), P (kolejność = od soli nazwy misji, cyklicznie) | `music_allowed_mask` z `core/audio` |
| Głośności, mowa/napisy, dźwięki interfejsu, pętle stanów (nagłówek sekwencji), dźwięki budynków/pocisków | nie zrobione | `--volume`, `--no-audio` |

# Przepisanie silnika (C++20): plan i stan

Cel: nowy silnik w czystym C++20, który wczytuje oryginalne dane gry (Aztec Empire, 1999) i zachowuje się tak samo,
ale z poprawkami (renderer GPU z interpolacją, lepszy pathfinding, szerokie ekrany, nowe funkcje).
Rekompilowana wersja (`runtime/` + `work/gen/`) pozostaje wzorcem do testów różnicowych, nie produktem.

## Zasady
- Kod nowego silnika jest napisany od zera na podstawie obserwacji zachowania i zrekonstruowanych struktur.
  Do repo nie trafia kod wygenerowany z binarki, binarka, ani zasoby gry.
- Symulacja jest deterministyczna (stały krok 12 Hz, bez zależności od czasu ściany, liczby zmiennoprzecinkowe
  tylko tam, gdzie oryginał też ich używa i z tą samą precyzją), bo multiplayer to lockstep.
- Każdy moduł symulacji powstaje z testem różnicowym względem wzorca (ten sam stan wejściowy + te same polecenia =
  ten sam stan wyjściowy, porównywany skrótem).

## Moduły docelowe
| Moduł | Zawartość | Źródło wiedzy |
|---|---|---|
| `core` | formaty plików, archiwa .xbe, sprite'y, mapy, ini (gotowe w `src/core`) | docs/formats.md |
| `sim` | świat, jednostki, budynki, ekonomia, walka, pathfinding, AI, polecenia | klasy z `tools/analyze_classes.py` |
| `render` | renderer GPU (SDL_GPU/OpenGL), interpolacja między tikami, mgła wojny, minimapa | funkcje rysujące + `docs/formats.md` |
| `ui` | menu, panel, odprawy, dialogi, nakładka F9 | oryginał + `runtime/overlay.c` |
| `audio` | miks, muzyka z CD (xbe), mowa | `runtime/dsound.c` |
| `net` | lockstep, IPX/UDP, lobby | `runtime/winsock.c`, protokół z kodu |
| `platform` | okno, wejście, zapis, ustawienia | `runtime/backend.c` |

## Narzędzia przygotowane
- `tools/analyze_classes.py`: odzyskuje klasy C++ z tablic wirtualnych (konstruktory zapisują adres vtable do `[reg]`):
  `python3 -I tools/analyze_classes.py game/Aztec.exe work/gen/gen_funcs.h work/classes.json`
- `tools/class_fields.py`: katalog pól per klasa (offset, typ, liczba odwołań) z dekompilatora:
  `python3 -I tools/class_fields.py work/classes.json work/decomp/all.c work/classes.md`
- `tools/coverage.py`, `tools/explore.sh`, `tools/monkey_all.sh`, `tools/mp_test.sh`: pokrycie funkcji i testy scenariuszowe.
- Wyniki (`work/`) są pochodnymi binarki i nie są commitowane.

## Co odkryto (2026-10-19)
- Gra ma 176 klas z tablicami wirtualnymi (brak RTTI). Rodzina ok. 20 klas po 75-88 metod wirtualnych, rozmiary
  100-250 bajtów, dziedziczy po klasie bazowej o vtable `0x48f2d8` (75 metod): kandydaci na jednostki, budynki i obiekty świata.
- Strona gracza (`Side`) to obiekt o rozmiarze 0x69c: `+0x640` suma wydana, `+0x688` wskaźnik na obiekt bazy, `+0x690` lista,
  `+0x698` złoto, `+0x140` flaga zwycięstwa.
- Gra rysuje scenę raz na tik logiki (12 Hz), kursor osobno (przechwytywany, rysowany przez backend).

- Fabryka obiektów (`0x43d5d0`, napis "obj_NewUnit"): 90 typów obiektów w tabeli `0x4933f8` (rodzaj, parametr), 11 rodzajów
  (ctor 0x433190, 0x44a350, 0x40fb70, 0x471390, 0x460090, 0x407360, 0x42bda0, 0x41cce0, 0x408410, 0x415f50, 0x42d9b0).
  Rodzaj 2 obejmuje 44 typy, rodzaj 0 17 typów. Narzędzie: `tools/object_kinds.py`.
- Obiekty żyją w pulach (alokator `0x4cc3d0`) w regionie VirtualAlloc 0x50000000+ oraz w stercie 0x10000000+.
  Komenda skryptu `objscan VTABLES OUT` (RT_SCRIPT) wypisuje żywe obiekty (adres, vtable); `heapdump PATH` zapisuje sterte.
  W misji 1 po 3 s: ok. 700 obiektów z 176 znanych vtable (najliczniejsze 0x490ab8: 148, 0x48f5e0: 96, 0x4932cc: 60).
  Jeszcze nie przypisano klas do ról (jednostka, budynek, drzewo, pocisk).

- Pule obiektów (klasa z vtable `0x4953f8`): nagłówek `+4` baza areny, `+8` koniec użytego obszaru, `+0x10` stride (8 bajtów nagłówka
  listy + dane), `+0x14` pojemność, `+0x18` liczba żywych. Pula jednostek: `0x4cc3d0` (stride 0xb8, pojemność 10000),
  pozostałe aktualizowane w ticku: `0x4cd698` (stride 0x30), `0x5156f8` (0x1c0), `0x4c1978` (0x40).
- Obiekt jednostki (klasa `0x4932cc` i pochodne, dane od `pool_elem+8`): `+0x24/+0x28` pozycja w pikselach świata (środek),
  `+0x44/+0x48` kafel (kafel = 48 px), `+0x6c/+0x70` pozycja stałoprzecinkowa (px*256), `+0x38/+0x3c` (np. 0x1770/0x1770) wartość
  zdrowia prawdopodobnie, `+0x80` kierunek/stan animacji, `+0x88` licznik animacji, `+0x30` wskaźnik do zestawu animacji.
- Tik logiki (`0x4577b0`): przetwarza polecenia stron (typy 0x12-0x24 z kolejek `0x43d120`), potem aktualizuje 3 pule przez metody
  wirtualne (+0x60, +0x8, +0x138) i strony (`0x449980`).
- Wyrocznia stanu: `RT_STATE_LOG=plik` zapisuje co tik: liczbę żywych obiektów i skrót FNV po 4 pulach (wskaźniki maskowane).
  Dwa niezależne przebiegi misji 1 bez wejścia zgadzają się w 211 z 214 ticków (3 różnice przejściowe, niewyjaśnione).

- Nazwy z napisów `assert` w exe: 58 plików źródłowych oryginału (`E:\SnakE\Empire\Source\*.cpp`), m.in. Marine (kind 0), Spider (1),
  Building (2), Zeppelin (3), Truck (4), Bird (5), Headtruck (6), Defence (7), Boat (8), Cannon (9), Johnka (10), Arrow, Axe, BallExpl, FireThrow,
  SplashDamage, Weapon, Tactic (AI), Town/TownsInfo, BuildTree, Harvest, Triggers/Conditions, MapExplorer, WaveSearch (pathfinding), Heapsort.
  Funkcje z nazwami w napisach: `wsr_WaveSearch` 0x46c470 (pathfinder: fala po siatce z tablicami przejezdności),
  `obj_NewUnit` 0x43d5d0, `obj_NewVoidUnit` 0x43e0b0, `wep_NewWeapon` 0x46d900, `wep_CalcKillTime(s)` 0x46fac0/0x46fbd0.
- Dokumentacja klas: `docs/classes.md`; nagłówki układów: `src/engine/model/objects.hpp` (generowane); lista kontrolna: `docs/REBUILD_CHECKLIST.md`.

- Pole `+0x50` każdego obiektu świata = id typu (UGI, 0..89); `+0x3c` = maks. zdrowie typu (np. 6000 piechur, 5000 muszkieter, 3500 łucznik,
  10000 rycerz, 36000 niedźwiedź); `+0x68` = wskaźnik do statycznego rekordu opisu typu (0x4c bajty, .data od 0x4ac428, wypełniany przy starcie).
- `docs/object_types.md`: 90 typów (nazwa UGI, klasa) i 45 typów pakietów/poleceń (PCT_*), w tym polecenia klatki (MOVE, ATTACK, BUILD, ...).
- Narzędzia testowe: `tools/make_ztest.py` + `tools/ztest_run.sh` (własna mapa testowa z jednostkami wybranych typów na stronie przeciwnika;
  budynki muszą powstawać przez sekcję [Town], w [Team] wywołują wywołanie pustego wskaźnika w `Building.cpp`).

- Struktura ticku logiki `0x4577b0` (pełniej): (1) polecenia stron, (2) aktualizacja pul w kolejności: `0x4c1978` (+0x60), `0x4cd698` (+0x8),
  `0x5156f8` (+0x138), strony `0x449980`, `0x4d9b50` (+0x138), jednostki `0x4cc3d0` (+0x120), `0x515a98` (+0x84), broń `0x4d9bbc[]` (`0x470c10`), `0x4c1298` (+0x8),
  (3) składanie list wyświetlania: dla obiektów z pul `0x4cc3d0`, `0x515a98`, `0x4c1978` wywołanie `0x47cb30(lista 0x4d9c10, pozycja, ..., sprite)` oraz dla warstw
  per strona `0x47d070`/`0x47d430` (lista `0x4d9c0c`); sprite bieżącej klatki zwraca metoda wirtualna +0x70/+0x74/+0x80/+0x5c, pozycję +0x14.
  Wniosek dla renderera: scena jest składana raz na tik; płynny ruch wymaga zapamiętania poprzedniej pozycji każdego obiektu i interpolacji przy rysowaniu.

## Etapy
1. [x] Narzędzia klas i pól, plan.
2. [ ] Rekonstrukcja struktur: klasa bazowa obiektu, lista obiektów świata, mapa kafli, polecenia.
3. [~] Wyrocznia: skrót stanu co tik jest (RT_STATE_LOG). Brakuje zapisu poleceń per tik (hook na 0x43d120) i migawek pul do testów modułów.
4. [ ] Renderer GPU z interpolacją (czyta stan gry z pamięci rekompilacji, potem ze `sim`).
5. [ ] Moduł `sim`: pathfinding + kolizje, następnie reszta, każdy z testem różnicowym.
6. [ ] Pokrycie funkcji niewykonywanych: scenariusze (misje 10-15, edytor, skirmish, AI w trudniejszych układach),
      a dla funkcji bez scenariusza: testy jednostkowe wywołujące je bezpośrednio na zrzutach stanu.
7. [ ] Zamiana modułów w rekompilacji na nowe, aż nic z lifteru nie zostanie.

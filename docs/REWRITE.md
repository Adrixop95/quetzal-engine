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

## Etapy
1. [x] Narzędzia klas i pól, plan.
2. [ ] Rekonstrukcja struktur: klasa bazowa obiektu, lista obiektów świata, mapa kafli, polecenia.
3. [ ] Wyrocznia: zapis/odtwarzanie poleceń na poziomie lockstep + skrót stanu co tik.
4. [ ] Renderer GPU z interpolacją (czyta stan gry z pamięci rekompilacji, potem ze `sim`).
5. [ ] Moduł `sim`: pathfinding + kolizje, następnie reszta, każdy z testem różnicowym.
6. [ ] Pokrycie funkcji niewykonywanych: scenariusze (misje 10-15, edytor, skirmish, AI w trudniejszych układach),
      a dla funkcji bez scenariusza: testy jednostkowe wywołujące je bezpośrednio na zrzutach stanu.
7. [ ] Zamiana modułów w rekompilacji na nowe, aż nic z lifteru nie zostanie.

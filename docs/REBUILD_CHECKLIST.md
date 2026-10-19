# Co jest potrzebne do przebudowy silnika (lista kontrolna)

Stan: [x] gotowe, [~] częściowo, [ ] brak.

## Dane
- [x] Archiwa `Res/*.xbe` (format znany, dekodery w `src/core`)
- [x] Sprite'y 0x44c/0x454 (flagi 1,2,4,8,0x40; `docs/formats.md`)
- [x] Mapy terenu `Maps/*.map`
- [x] Palety `Pal.xbe`
- [ ] Animacje (grupa 0x515)
- [ ] `.spr .vis .rm .dat .scn .mit` (przy mapie): rozpoznać pola (mgła/widoczność, obiekty startowe, skrypty)
- [~] `.mp` (INI z opisem mapy i jednostkami startowymi): format tekstowy, pola do spisania
- [ ] `strbase.bds` (napisy, nazwy jednostek)
- [ ] `Keys.ini`, `*.plr` (gracz), `single%i.sav` (zapis gry)
- [ ] Muzyka `Music.xbe` (RIFF) i `Snd.xbe` (WAV): dekodery są w runtime, brak w `src/core`

## Model obiektów
- [x] Klasy z tablic wirtualnych: 176 (`docs/classes.md`), nazwy ze ścieżek źródłowych (58)
- [x] Hierarchia (`tools/class_tree.py`), rozmiary i układ pól z obserwacji (`src/engine/model/objects.hpp`)
- [x] Fabryka obiektów: 90 typów, 11 rodzajów
- [ ] Znaczenie pól poza pozycją/zdrowiem/animacją
- [ ] Znaczenie 70-88 metod wirtualnych klasy bazowej (VisibleThing) i klas pochodnych
- [ ] Tabele statystyk typów (koszt, zdrowie, prędkość, zasięg): skąd są ładowane
- [ ] Polecenia graczy (typy 0x12-0x24): pełna lista i dane

## Symulacja
- [~] Tik logiki `Test.cpp`/`0x4577b0` opisany z grubsza
- [ ] Pathfinding: `WaveSearch.cpp` (0x46bb70, 0x46bdb0, 0x46cdc0) i `Heapsort.cpp` (0x473030)
- [ ] AI: `Tactic.cpp`, `Defence.cpp`, `Triggers.cpp`, `Conditions.cpp`
- [ ] Ekonomia: `Harvest.cpp`, `Town.cpp`, `TownsInfo.cpp`, `BuildTree.cpp`
- [ ] Walka: `Weapon.cpp`, `SplashDamage.cpp`, pociski (`Arrow`, `Axe`, `BallExpl`, `FireThrow`, `Cannon`)
- [ ] Mgła wojny / widoczność: `MapExplorer.cpp`, `.vis`
- [ ] Generator liczb losowych (determinizm lockstep)

## Reszta
- [~] Renderer (sprite'y, teren, cienie, mgła): funkcje `display.cpp`, `sprite.cpp`, `color.cpp`, `View.cpp`
- [ ] Interfejs (85 klas widgetów)
- [~] Sieć: protokół pakietów lockstep (do spisania z `0x43a410`)
- [ ] Dźwięk 3D (`3DSound.cpp`, `snd.cpp`, `mix.cpp`)

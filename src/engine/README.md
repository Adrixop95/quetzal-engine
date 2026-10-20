# Silnik C++20 (src/engine)

Czysta reimplementacja, wczytuje oryginalne dane z `game/` (nic z gry ani z exe nie trafia do repo).
Cel CMake: `aztec_engine` (biblioteka `aztec_engine_lib` + `src/engine/app/main.cpp`), testy: `test_engine`.

## Uruchomienie
    cmake -S . -B build_engine && cmake --build build_engine -j --target aztec_engine test_engine
    build_engine/aztec_engine --game game 1            # misja 1; też: 3, Mission_4, Pentagram, ścieżka do .mp
    build_engine/aztec_engine --list                   # lista map
    build_engine/aztec_engine 1 --wander --borderless  # jednostki chodzą losowo (test interpolacji 12 Hz)
    build_engine/aztec_engine 1 --size 1920x800 --screenshot out.png --cam 40,40 --zoom 1.5
    build_engine/test_engine game
Klawisze: WASD/strzałki, scroll przy krawędzi, środkowy/prawy przycisk = przeciąganie, kółko = zoom,
LPM na minimapie = skok kamery, F11 borderless, Alt+Enter fullscreen, F1 pomoc, F2 siatka, F3 przejezdność,
F4 etykiety, F5 znaczniki, F6 edge scroll, F7 nakładki terenu, F10 HUD, Spacja pauza, F12 zrzut, Esc wyjście.

## Moduły
- `platform/`: `Window` (okno/borderless/fullscreen, vsync, dowolna rozdzielczość), `Input`, `FixedStepper` (stały krok 12 Hz).
- `assets/`: `GameData` (archiwa, wyszukiwanie plików bez rozróżniania wielkości liter), `mission` (parser .mp, cp1250 -> UTF-8),
  `sprite_provider.hpp` (interfejs mapowania typ -> sprite).
- `world/`: `World`/`Object` (pozycje stałoprzecinkowe px*256, `prev_*` i `cur_*` per obiekt = interpolacja), `Camera`
  (gładki scroll, kroki zoomu, clamp), `type_info` (nazwy i rodzaje 90 typów).
- `sim/`: interfejs `Simulation` (tik 12 Hz; sim pisze tylko `cur_*`, driver robi `World::begin_tick()`), `NullSimulation`, `WanderSimulation`.
- `render/`: `Atlas` (strony 2048^2, pakowanie półkami), `SpriteStore` (leniwe dekodowanie z .xbe), `QuadBatch`
  (SDL_RenderGeometry, 1 wywołanie na stronę atlasu), `SceneRenderer` (teren + nakładki + obiekty), `Font` 3x5, zapis PNG bez zależności.
- `ui/`: `Cursor` (programowy, rysowany na końcu każdej klatki z aktualnej pozycji myszy), `Minimap`, HUD.
- `app/`: opcje wiersza poleceń, główna pętla (render co vsync, symulacja w stałym kroku).

## Podpięcie sprite'ów
Zaimplementuj `SpriteProvider::pick(ObjectState, SpriteDraw&)` (lub wypełnij `TableSpriteProvider`) i przekaż do
`run_engine(opts, provider)`. `SpriteKey` = archiwum + grupa (0x44c/0x454) + indeks + paleta + shade. Pozycja rysowania =
pozycja obiektu (środek kafla) + `(ox, oy)` z nagłówka sprite'a + `(dx, dy)` z `SpriteDraw`. Obiekty bez sprite'a dostają
znacznik w kolorze strony z numerem typu. Dane mapowania: `src/engine/data/`, dokument `docs/sprites_mapping.md`.

## Znane braki
- Kafle terenu o indeksach ok. 1337-1383 (przejścia/dither) wyglądają na prążkowane: prawdopodobnie wymagają innego sposobu
  rysowania (mieszanie/cieniowanie), nie zbadane.
- Paleta podglądu minimapy (.spr) niepotwierdzona (używana paleta terenu 4/14); kolory stron (`Color` 0..7) to wartości zastępcze.
- Brak mgły wojny, cieni, animacji, dźwięku, sieci, logiki gry (tylko interfejs `Simulation`); kursory oryginalne niewczytane.
- Paleta Tiles1 (TileSet=1) niesprawdzona (domyślnie jak Tiles0; opcja `--palette`).
- Hotspot sprite'a (`ox,oy`) interpretowany jako przesunięcie lewego górnego rogu względem pozycji obiektu; do potwierdzenia.

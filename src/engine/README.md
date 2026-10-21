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

## Sprite'y i dane (stan po drugiej rundzie)
- `AnimSpriteProvider` (`assets/anim_provider.*`) oparty na `data/sprites.hpp`: typ + stan (idle/move/attack/die/fidget) + kierunek 0..7 + tik animacji
  -> sprite z `Units.xbe` (ciało 0x44c, cień 0x454 rysowany pod spodem jako półprzezroczysta sylwetka). Animacja liczy `Object::anim_tick`
  (rośnie w `World::begin_tick()`, czyli w stałym kroku 12 Hz); pozycja nadal jest interpolowana między tikami.
- Hotspot sprite'a: lewy górny róg = pozycja - (ox, oy) (+ `yoff` klatki). Zgadza się ze zrzutem oryginału (położenie budynku względem jednostek).
- Paleta sprite'a z nagłówka (+0xC): Tiles0 używa Pal 4/5/6 (trawa/piasek, woda, brzeg), Tiles1 8/9/10, Units: id < 4 = paleta drużyny
  `id + 0x10 + kolor*4`. Sprite'y z flagą 0x40 to indeksowane RLE z alfą (`core/sprite_alpha.*`), nie 16-bit kolor.
- Kafle z `h > cell_h` to paski animacji (np. woda 8 klatek po 48 px): klatka = `(tick/2) % n`. Warstwa nakładek `.map` (górne 16 bitów) to drzewa/skały:
  ciało w 0x44c, cień w 0x454 pod tym samym id, komórka 192x192 x 4 warianty, sortowane po y razem z obiektami.
- Budynki z `[Town#]` dodawane są do świata (typ z `Type` i `Race`); budynek gotowy = slot 1 animacji (slot 0 = etapy budowy).
- Mgła wojny: `.vis` warstwa strony lokalnej (`World::local_side`), tekstura 1 texel/kafel z filtrem liniowym; obiekty innych stron w ciemności ukryte;
  F8 przełącza, `World::fog_version` do odświeżania przez przyszłą symulację. Minimapa też ma mgłę.
- Kursory z `common.xbe`: pióro (id 1, domyślny), miecz (0, atak, wskazanie wroga), buty 6..13 = strzałki przewijania krawędzią (hotspot na krawędzi),
  maczuga 2 i włócznia 5 (Move/Pan, przypisanie niepewne), młotki 3 (nieużyte), czerwone X 14..21 (nieużyte); animacje 80 ms/klatka jak w oryginale.
- Panel prawy: `common.xbe` id 292 (204x493, paleta 48), pasek dolny powtarzany przy wyższych oknach; minimapa wpisana w okno panelu, kamera ma szerokość okna
  minus panel. Przyciski/lista budowy/portrety nie są zrobione (tło panelu tylko).
- Narzędzie: `aztec_engine --game game --sprite-sheet common:0-21 out.png` wypisuje arkusz obrazków z archiwum (units|tiles0|tiles1|common).

## Podpięcie własnych sprite'ów
Zaimplementuj `SpriteProvider::pick/pick_shadow` (lub wypełnij `TableSpriteProvider`) i przekaż do `run_engine(opts, provider)`;
domyślnie używany jest `AnimSpriteProvider`, `--no-sprites` wymusza znaczniki zastępcze.

## Oświetlenie, czcionki, UI (trzecia runda)
- Oświetlenie dnia: tablice kluczy (czas, RGB, poziom ambient) są czytane w czasie uruchomienia z `game/Aztec.exe` (`assets/daylight.*`; w repo nic z exe nie leży).
  Zegar = `StartTime` + tiki symulacji (doba 7200 tików), klucze interpolowane liniowo (oryginał: skokowo). Sprite'y są pieczone na poziomie 14 i modulowane kolorem
  (tint * jasność poziomu ambient+1 względem 14); zgodność ze zrzutem oryginału: piasek ok. (178,128,111) vs (201,124,112), trawa ok. 1.3x jaśniej. Poziom +1 i brak
  mapy światła per kafel to przybliżenie. `--no-light`, `--time T`.
- Czcionki: `fnts.xbe` baza 1200 (małe, antyaliasowane, paleta 53), glif = baza + bajt cp1250; wbudowany 3x5 zostaje jako zapas i do etykiet zastępczych.
- `core/strbase.*`: parser `strbase.bds` (UTF-8, test na prawdziwym pliku). `ui/names.*`: nazwy typów (ordinal 166-210 po nazwie).
- Zaznaczanie: klik lub ramka LPM (tylko własne jednostki; shift dodaje), pierścień zaznaczenia (common 309/314, animowany), pasek zdrowia (maks = najwyższe hp tego typu
  w misji), podsumowanie w panelu, tooltip przy najechaniu ("Nazwa\nPrice: %i Time: %i" jeśli jest `docs/data/types.csv` lub `--rules`).
  Brak przycisków poleceń, listy budowy i portretów (nie ma jeszcze symulacji poleceń). `--select X0,Y0,X1,Y1` zaznacza w trybie zrzutu.
- `.mp` czytane przez `core/mp.*`; `assets/mission.*` to cienki adapter.

## Symulacja (czwarta runda)
- `sim/game_sim.*`: deterministyczna symulacja 12 Hz (liczby całkowite), polecenia z `sim/commands.hpp` (numery = PCT_FRAME_*), hash stanu (`state_hash`), log poleceń
  (`serialize_log`) i odtwarzanie (`GameSim::replay`) = migawka/przywracanie stanu. Testy (`test_engine game`): ten sam ciąg poleceń daje ten sam hash, inny - inny,
  replay daje identyczny hash, walka, trening jednostki.
- Reguły i dane: `sim/rules.*` czyta `docs/data/*.csv` w czasie uruchomienia (`--rules-dir`); tabela, co jest zweryfikowane a co zastępcze: `docs/engine_rules_status.md`.
- Ruch: `sim/path/` (agent ruchu od agenta pathfindingu), `GameSim` używa tylko `MoveSystem`.
- Sterowanie (`ui/commands_ui.*`): PPM = ruch/atak (przeciągnięcie PPM = przesuwanie mapy), `A` + LPM = atak w marszu, `H` = stój, Ctrl+0-9 / 0-9 = grupy,
  `P` = pauza symulacji, PPM na minimapie = ruch. Panel: menu produkcji z drzewa budowy (jednostki treningowe i budynki `[B]` stawiane kliknięciem na mapie), kolejka, tooltip.
- Opcje testowe: `--cmd-move X,Y` / `--cmd-attack-move X,Y` (dla `--select`), `--no-sim`, `--wander`.

## Znane braki
- Brak mapy światła per kafel (źródła światła, maski light.xbe), maski nie są nakładane.
- Kierunek początkowy jednostek (silnik: południe), stany attack/die/fidget i sloty 32-kierunkowe (ciężarówki, ptaki) nieobsłużone poza idle/move.
- Uszkodzenia i budowa budynków, pociski, efekty, drugi sprite (wieżyczka/broń) nie są rysowane.
- Mapa `.rm` wczytana, nie używana.
- Brak AI wroga, skryptów `.scn`, garnizonu/naprawy, dźwięku i sieci; reszta reguł wg `docs/engine_rules_status.md`; kolory stron (`Color` 0..7) dla znaczników zastępczych są arbitralne.
- Zrzut referencyjny rekompilacji (800x600, misja 1) porównany ręcznie: układ kamery/obiektów zgodny, różnice = oświetlenie i panel (przyciski).

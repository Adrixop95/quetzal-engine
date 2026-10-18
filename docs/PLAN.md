# Aztec Empire: plan odtworzenia

Cel: grywalny build Aztec Empire (New Media Generation, 1999) na Linux/Windows/macOS, z poprawkami (widescreen).
Repo nie zawiera Aztec.exe, assetów ani kodu pochodzącego z binarki. Użytkownik dostarcza własną instalację gry.

## Strategia: statyczna rekompilacja
1. `tools/ghidra_scripts/LiftAll.java` (Ghidra headless) zamienia P-code każdej funkcji x86-32 na C
   (`work/gen/gen_*.c`). Plik wynikowy jest pochodną Aztec.exe i nigdy nie trafia do gita.
2. `runtime/` to ręcznie pisany runtime: 4 GiB pamięci gościa, loader PE, dyspozytor funkcji,
   shimy WinAPI (win32.c) i DirectX na SDL2 (ddraw.c, dsound.c, dinput.c, backend.c).
3. `src/core`, `src/app`, `src/tools`: własne dekodery formatów (archiwa .xbe, sprite'y, palety, mapy)
   i przeglądarka SDL2. Służą do weryfikacji i do przyszłej czystej reimplementacji.

## Etapy
- [x] Formaty zasobów rozpoznane i zweryfikowane (docs/formats.md).
- [x] Lifter: 100% funkcji kompiluje się do C.
- [~] Runtime: uruchomienie startu CRT i WinMain, kolejne brakujące shimy i adresy.
- [ ] Grywalny tryb jednego gracza na Linuxie (menu, misje, dźwięk, wideo Indeo przez ffmpeg).
- [ ] Widescreen: SetDisplayMode/viewport (patch stałych w grze lub reimplementacja widoku).
- [ ] Windows i macOS (VirtualAlloc zamiast mmap, long double na ARM).
- [ ] Multiplayer (protokół sieciowy z kodu `Network`).
- [ ] Stopniowa zamiana zlifowanych funkcji na czysty C++ (moduł po module, wg ścieżek z `map_modules.py`).

## Iteracja
Nieobsłużone API lub adres kończy się komunikatem `FATAL`. Brakujące adresy trafiają do
`tools/extra_entries.txt`, brakujące shimy do `runtime/`.

## Stan (2026-10-18)
Zlifowana gra uruchamia się na Linuxie i renderuje menu główne (800x600, 16 bpp, SDL2).
Rozwiązane po drodze: odkrywanie funkcji (wskaźniki w danych i stałe w kodzie), DF nie może być eliminowana
jako martwa flaga, DirectDraw2 (SetDisplayMode ma 5 argumentów), wirtualny rejestr, MCI dla filmów,
GetVersionExA musi walidować rozmiar struktury.
Diagnostyka: RT_TRACE_API, RT_TRACE_WAIT, RT_DUMP_DIR (zrzut klatek PPM), RT_DUMP_SURF, RT_WATCH_ADDR,
-DAZTEC_TRACE_CALLS=ON (licznik wejść do funkcji gościa), -DAZTEC_HEAPDEBUG=ON.
Do zrobienia: filmy (ffmpeg), sieć, dźwięk (sprawdzić), widescreen, Windows/macOS, drugi dysk (muzyka CD2).

## Tryb testowy (szybki)
`RT_TIMESCALE=3 RT_SCRIPT=scenariusz.txt RT_DUMP_DIR=katalog ./build/aztec_recomp game/Aztec.exe`
Komendy skryptu (czas w ms czasu gry): `wait MS`, `move X Y`, `click X Y`, `rclick X Y`, `down`, `up`,
`key DIK`, `keydown DIK`, `keyup DIK`, `shot` (zapis klatki shot_NN.ppm), `quit`.
Scenariusz "utwórz gracza" trwa ok. 5 s zamiast 25 s. Pliki graczy (*.plr) powstają w katalogu gry.

## Przejście od menu do misji (2026-10-18)
Ścieżka: menu (Wybierz gracza) -> Gra pojedyncza -> Kampania -> Graj misję -> odprawa -> "Start Game" (tarcza) -> mapa misji 1.
Układ katalogu gry (jak na płytach): Maps/ (mapy instalacyjne + misje z obu CD), Video/, Music/, Res/.
Wirtualny rejestr podaje ścieżki "D:\Maps", "D:\Video", "D:\Music" (gra używa pierwszych 3 znaków jako "napędu CD").

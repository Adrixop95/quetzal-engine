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

## Widescreen (2026-10-18)
Misje renderują się w dowolnej rozdzielczości (zweryfikowane 1280x720): `./run_aztec.sh --res 1920x1080`.
Mechanizm: gra trzyma tablicę rozdzielczości (0x515510) i białą listę trybów (0x495498); runtime podmienia wpis 0
oraz 1280x1024 na żądany rozmiar i dodaje go do `EnumDisplayModes`. Widok mapy skaluje się, panel boczny
zostaje stałego rozmiaru (kosmetyka: pusty obszar pod panelem). Menu (800x600) jest wycinane w backendzie, gdy reszta
powierzchni jest czarna. Nie sprawdzone: bardzo duże rozdzielczości (bufory statyczne gry).

## Menu pomocnicze (F9) i cheaty
Nakładka SDL (runtime/overlay.c, czcionka z tools/gen_font.py) otwierana klawiszem F9 lub `--menu`.
Ustawienia (zapis w game/aztec_settings.ini): rozdzielczość, tryb okna (okno / bez ramki / pełny ekran, na żywo),
skalowanie (ostre / gładkie), filmy. Zmiana rozdzielczości wymaga ponownego uruchomienia (przycisk w menu).
Pomoc w grze: "Odblokuj wszystkie misje" (wywołuje funkcję gry 0x43f560(15): zapisuje postęp 15 w pliku gracza *.plr,
zweryfikowane) i "Natychmiastowe zwycięstwo" (ustawia flagę jak wbudowany klawisz CheatWin: DAT_004dd2f4+0x140+idx*0x69c,
zweryfikowane baner WYGRANA). W Keys.ini istnieje też wbudowana akcja `CheatWin <klawisz>`.
Niezrealizowane: dodawanie złota (pole nie jest zwykłym int/float/double w tablicy stron; do dalszego śledzenia komendami
skryptu snap/diff/fsnap/ddiff w backend.c).
Komendy skryptu testowego: wait, move, click, rclick, down, up, key, keydown, keyup, shot, quit, f9, ovclick X Y,
cheat unlock|win, scan/scan16/snap/diff/fsnap/fdiff/fdiffd/fdifff/dsnap/ddiff/poke/pokeb/pokeside/dumpside (diagnostyka pamięci).

## Mysz: wirtualny kursor (2026-10-18)
Wskaźnik systemowy jest zablokowany w oknie (SDL relative mouse mode), więc nie ucieka na żadnym kompozytorze.
Ruch względny jest całkowany do wirtualnego kursora we współrzędnych obrazu (niezależnie od skalowania i pasów
przy proporcjach), z czułością z menu F9 (`mouse_speed` w aztec_settings.ini, domyślnie 100%).
Pozycja jest wpisywana do zmiennych kursora gry (0x4c5468/0x4c546c, 0x4c4f50/0x4c4f54), nakładka F9 rysuje własną strzałkę.
Debug: `tools/debug_run.sh` (log w debug/<czas>/debug.log, kategorie mouse/window/render/dinput/overlay/state).

## Freeze w misji: przyczyny (2026-10-19)
1. Brak Winsock: gra prowadzi blokadę kroków (lockstep) przez warstwę sieciową nawet w single player. Bez `WS2_32.dll`
   wątek sieciowy kończył się błędem i symulacja nigdy nie startowała. Teraz runtime/winsock.c daje prawdziwe gniazda UDP
   (przy okazji LAN) oraz gniazdo IPX z pętlą zwrotną (gra wysyła sobie pakiet próbny i czeka na jego odbiór).
2. Błąd w `timeGetTime`: ujemna różnica nanosekund rzutowana na uint64 dawała skoki czasu o miliardy ms, a pętla ticków
   liczyła z tego `Sleep(140462661)`. Zawsze liczyć czas w int64.
3. Wcześniej poprawione: mutexy z właścicielem, VirtualAlloc z odzyskiem, precyzja x87 53 bity (RT_FPU53).
Diagnostyka: log `netstate` (session/frame) w debug.log, `[sleep]` dla Sleep > 300 ms, `[wait]` BEGIN/END dla nieskończonych oczekiwań,
`[event]` dla Create/Set/Reset/PulseEvent.

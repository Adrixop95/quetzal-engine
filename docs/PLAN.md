# Aztec Empire - plan odtworzenia silnika

Cel: wierna reimplementacja (Aztec.exe 1999/2000, New Media Generation) w C++17 + SDL2,
działająca na Linux/Windows/macOS. Bez assetów w repo: gracz wskazuje własną instalację.

## Zasady
- Repo zawiera tylko własny kod, specyfikacje formatów i narzędzia. Nie ma w nim Aztec.exe, assetów ani surowego outputu dekompilatora.
- Surowa dekompilacja żyje w `work/` (poza gitem). Do repo trafia kod przepisany ręcznie.
- Weryfikacja: porównanie zachowania z oryginałem pod wine (zrzuty stanu, pliki zapisów, replaye).

## Fazy
1. Formaty: archiwa Res/*.xbe, mapy (.map/.dat/.rm/.spr/.vis/.scn/.mp/.mit), strbase.bds, Keys.ini, zapisy. Narzędzia CLI + testy na plikach z płyt.
2. Platforma: okno SDL2, paleta 8-bit (jeśli tryb indeksowany), wejście, dźwięk, wideo (ffmpeg/Indeo), logika czasu.
3. Renderer: kafle, sprite'y, mgła/światło, UI. Viewport niezależny od rozdzielczości (widescreen).
4. Silnik gry: obiekty, jednostki, budynki, AI, pathfinding, skrypty scenariuszy (.scn), kampania.
5. Multiplayer: protokół sieciowy (DirectPlay/IPX/TCP?), do ustalenia z kodu.
6. Edytor, menu, zapisy, polerka, CI na 3 platformy.
7. Weryfikacja 100%: pokrycie funkcji z functions.txt (tabela: funkcja -> moduł -> status).

## Śledzenie
`docs/coverage.md`: lista funkcji Ghidry i status (todo/zrobione/nie dotyczy).

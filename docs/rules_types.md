# Reguly i dane typow (stan czesciowy)

Tabele liczbowe: `docs/data/*.csv`, generowane przez `python3 -I tools/extract_rules.py [work/heapdump.bin]`
(statyczne tablice z `Aztec.exe` + deskryptory wypelniane przy starcie gry, czytane ze zrzutu `heapdump`).

## Zweryfikowane (z kodu, odczyt tablic)
- **Cennik i czas** (Town.cpp, statyki wypisywane przez funkcje zrzutu `0x45ba56`): `UnitPrices[90]` @0x4bc3e8, `BuildTime[90]` @0x4bc550
  (statyczne, w pliku exe). `DevLevel[90]` @0x515580 liczone w konstruktorze Town (0x459490) funkcja `0x45c780` z drzewa budowy
  (0 = typy bazowe, 1 = farm/kopalnia i podobne, rosnie z liczba wymaganych budynkow, 10 = niedostepne; typy 82..89 = 10). Plik: `types.csv`.
- **Mnozniki** (`town_factors.csv`): `Ktime[3] = 2, 1, 0.667`, `Kprice[3] = 0.667, 1, 2` (indeks = BuildSpeed miasta, Town+0x158),
  `KWarehouse = 1.0, 1.333`, `KAdvPostTime = 1.5`? (patrz uwaga nizej), `KAdvPostPrice = 1.0`.
  Cena (0x45c690) = `round(UnitPrices[t] * K * f)`, czas (0x45c5b0) = `round(BuildTime[t] * K * f)`, `f = 1.0`, a `0.8` gdy strona miasta jest
  komputerowa i nie jest wrogiem wszystkich (`0x43d2c0` + `0x447320`). Zaokraglenie: `+ stala z 0x494838` (0.5) i `__ftol`.
  UWAGA: kolejnosc KAdvPost* przy odczycie z dumpu bywa odwrocona; sprawdz w 0x45c5b0/0x45c690 (czas uzywa 0x4bc3e4/0x4bc3e0 zaleznie od galezi).
  Ta sama funkcja daje cene/czas w tooltipie UI ("Price: %i Time: %i", 0x441ef6), wiec jest to cena widoczna dla gracza.
- **Drzewo budowy** (`buildtree.csv`, tablica 0x4af130: 3 rasy x 4 typy ratusza x 33 wpisy x 34 dwordy; ratusz: 0 forpost, 1 wioska, 2 miasto,
  3 forteca; `townhall_ids.csv`). Wpis: dword0 = typ (bit 0x4000000 = "buduj na mapie" tj. budynek, brak = trenuj jednostke);
  nastepne dwordy = wymagane budynki (bit 0x8000000 = budynek obcej rasy/"zaawansowany", np. DEMOLISHER wymaga LABS; RUS TRUCK_FLAMETHROWER
  wymaga zdobytego AZT_WORKSHOP). Koniec listy: 0xffffffff lub >= 0x5a. Odczyt: BuildTree.cpp (0x411890 kopiuje drzewo rasy, 0x45c780 oblicza DevLevel).
- **Deskryptory typow** (tablice w `.data`, wypelniane przez kod startowy; `desc_*.csv`, kolumny = dword d0..):
  Marine 0x4ac430 x0x4c (16), Building 0x49b538 x0x508 (32), Defence 0x4aa3b0 x0x110 (9), Headtruck 0x4ab2b0 x0x50 (8), Truck 0x4bc738 x0x44 (1),
  Spider 0x4ae378 x0x4c (2), Zeppelin 0x4bcd00 x0x44 (2), Bird 0x49b370 x0x58 (3), Cannon 0x4a8e18 x0x40 (1). Indeks = parametr z `types.csv`.
  Pole obiektu `+0x68` (Marine) / `+0x64` (Building, Bird, Zeppelin) / `+0x6c` (Defence, Headtruck, Spider) wskazuje deskryptor.
- **Marine** (`Marine::Think` 0x433db0): d0 = id animacji; d1 = HP poczatkowe (i prog regeneracji w jednym stanie), d2 = regeneracja HP na tik,
  d3 = prog regeneracji w drugim stanie, d4 = HP maks.; d5 = promien dojscia/krok ruchu w px na tik (ruch: pozycja 8.8 += wektor, dojscie gdy dystans <= d5);
  d7 = id broni (indeks do `0x515ac0[]`, -1 = brak; `Marine+0xa8` zwraca obiekt broni); d12..d14 = mnozniki 8.8 (256 = 1.0), znaczenie nieustalone
  (prawdopodobnie odpornosc wg klasy obrazen).
- **Building** (0x40fb70): d0 = id animacji, d1 = HP (np. 30000), d2/d3 = przesuniecie, d4/d5 = rozmiar (px); d0xf8 != 0 tworzy obiekt strzelajacy
  (FUN_0042afe0), d0x100 = id broni lub -1; d0x104..0x128 = progi rosnace (1000, 2050, 3100...; tylko farmy: RUS_FARM param 7, AZT_FARM param 20,
  prawdopodobnie przychod/poziomy produkcji). Reszta pol: nieustalona.
- **Bron** (Weapon.cpp): `wep_NewWeapon(owner, id)` (0x46d900) wybiera klase wg tabeli `0x494d30[id] = (klasa 0..13, parametr)` (`weapons_ids.csv`, id 1..59,
  obiekty prototypowe w `0x515ac0[]` tworzone z owner = NULL w 0x470780). Metody wirtualne broni: `+0x5c` obrazenia (atakujacy, cel), `+0x60` czas przeladowania,
  `+0x78` CanFire. `wep_CalcKillTime` (0x46fac0) = `ceil(HP_celu / obrazenia) * przeladowanie` (HP celu: vcall +0x38, a gdy atakujacy ma flage +0xf8: +0x78).
  Tabele danych klas (`weapon_class_*.csv`; rozmiar rekordu z `lea` w konstruktorach, liczba rekordow i znaczenie pol NIEZWERYFIKOWANE):
  klasa0 @0x48f868 x20, klasa1 (Arrow) @0x48f408 x64, klasa3 (Axe) @0x48f750 x24, klasa7 @0x494218 x36, klasa9 @0x493f60 x32, klasa12 @0x4906f0 x12,
  klasy 2/13 @0x494a90 x0x38, klasa8 @0x4ae448 x16. Klasy 4,5,6,10,11 nie maja tabeli parametrow. W klasie 1 pole d12 = id broni wtornej (wybuch).
- **Strona** (Side, 0x69c): `+0x698` gotowka, `+0x63c` suma zarobiona (`AddMoney` 0x4477d0), `+0x640` suma wydana (`TakeMoney` 0x447d00:
  odmawia gdy gotowka < kwota), `0x447d30` = CanAfford. `+0x648/0x64c/0x650` maski bitowe stron: sojusznik / wrog / neutralny
  (`0x447320` = isEnemy(x): x = -1 -> brak neutralnych/sojusznikow; x == wlasny indeks -> false).
- **Polecenie BUILD** (PCT 0x1c, tik 0x4577b0): znajduje wpis w drzewie miasta (`0x45cec0`), sprawdza (`0x45cd50`), stawia budynek (`0x410960`/`0x41d6a0`);
  HP budynku w trakcie budowy rosnie w polu +0x38 (`Building` 0x410c30/0x410cc0).

## Niezrobione / nieustalone (uczciwie)
- Znaczenie wiekszosci pol deskryptorow (zasieg, wzrok, pancerz, szybkosc dla nie-Marine), tabele obrazen (potrzebne czytanie metod +0x5c/+0x60 kazdej klasy broni),
  dochod (Harvest.cpp, `GetHarvestedPerFrame` 0x45c270 = `(gotowka/ +0x668 ...)`), limity populacji, naprawa/garnizon, poziomy trudnosci, Tactic/Defence/Triggers.
- Brak eksperymentow potwierdzajacych liczby (predkosc w px/tik, czasy zabijania). `docs/data/desc_*.csv` sa surowym zrzutem z jednej misji
  (misja 1) i moga zawierac wartosci zalezne od misji; zweryfikuj drugim zrzutem.
- Wartosci `KAdvPost*` i galezie 0.8 wymagaja testu w grze.

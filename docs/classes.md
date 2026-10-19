# Klasy gry (odzyskane z kodu i obserwacji)

Wygenerowane przez `tools/gen_class_docs.py` z: tablic wirtualnych, konstruktorów, łańcuchów źródłowych (assert) i skanu żywych obiektów
z 15 misji kampanii (`tools/survey.sh`). Nazwa = nazwa pliku źródłowego (`Marine.cpp` itd.) znaleziona w napisach pliku wykonywalnego; puste = brak śladu.

| vtable | nazwa | rola | baza | metod | żywych obiektów (15 misji) | rozmiary | misji |
|---|---|---|---|---|---|---|---|
| 0048f298 |  |  | 0049313c | 5 | 267 | 40,pool | 13 |
| 0048f2b4 |  |  | 004907c0 | 9 | 18 | pool | 5 |
| 0048f2d8 | VisibleThing |  | 004932cc | 75 | 21 | 176,pool | 3 |
| 0048f558 | Arrow |  | 0048f63c | 34 | 275 | 264,pool | 15 |
| 0048f5e0 | VisibleThing |  | 0048f63c | 23 | 5524 | 176,264,304,56,pool | 15 |
| 0048f63c | BallExpl |  | 0048f5e0 | 34 | 0 |  | 0 |
| 0048f6c4 | VisibleThing |  | 0048f63c | 34 | 2 | 264 | 2 |
| 0048f7e0 | Axe |  | 0048f63c | 34 | 274 | 264 | 15 |
| 0048f8f4 | BallExpl |  | 0048f63c | 34 | 315 | 264,pool | 15 |
| 0048f97c | Bird | world object kind 5 | 004936c8 | 75 | 207 | 176 | 7 |
| 0048faa8 | Boat | world object kind 8 | 004936c8 | 75 | 56 | 176 | 4 |
| 0048fbd4 | Tactic |  | 004944c0 | 83 | 56 | 256,pool | 4 |
| 0048fd20 | Building | world object kind 2 | 0048fe58 | 78 | 1499 | 176,pool | 15 |
| 0048fe58 | Defence | world object kind 9 | 004936c8 | 75 | 0 |  | 0 |
| 0048ff84 |  |  |  | 12 | 477 | 440,pool | 15 |
| 0048ffb4 | BuildTree |  |  | 19 | 481 | 5236 | 15 |
| 00490000 |  | UI widget |  | 28 | 36 |  | 12 |
| 00490070 |  | UI widget | 00490000 | 29 | 0 |  | 0 |
| 004900e4 |  | UI widget | 00490000 | 29 | 0 |  | 0 |
| 00490158 |  | UI widget | 00490000 | 29 | 8 |  | 1 |
| 004901cc |  | UI widget | 00490000 | 30 | 2 |  | 1 |
| 00490244 |  |  | 00493ef8 | 13 | 0 |  | 0 |
| 00490278 |  | UI widget | 004901cc | 30 | 0 |  | 0 |
| 004902f4 |  | UI widget | 00490000 | 33 | 0 |  | 0 |
| 00490380 | Cannon | world object kind 9 | 0048fe58 | 75 | 54 | 176 | 3 |
| 004904ac | Decoration |  | 0048f5e0 | 25 | 15 | 56,pool | 5 |
| 00490520 |  |  |  | 13 | 0 |  | 0 |
| 00490554 |  |  |  | 13 | 90 | 20 | 15 |
| 00490588 |  |  |  | 13 | 675 | 20 | 15 |
| 004905bc | Defence | world object kind 7 | 0048fe58 | 77 | 941 | 176,pool | 11 |
| 00490738 | VisibleThing |  | 0048f63c | 34 | 270 | 264,pool | 15 |
| 004907c0 | Triggers |  | 004940d0 | 9 | 0 |  | 0 |
| 004907f0 |  |  | 00490a48 | 31 | 45 | 56 | 15 |
| 0049086c |  |  | 00490ab8 | 31 | 90 | 56 | 15 |
| 004908e8 |  |  | 00490ab8 | 28 | 315 | 40 | 15 |
| 00490958 |  |  | 00490a48 | 30 | 45 | 84 | 15 |
| 004909d0 |  |  | 00490a48 | 30 | 135 | 84 | 15 |
| 00490a48 |  |  | 00490ab8 | 28 | 1710 | 40 | 15 |
| 00490ab8 |  |  | 004909d0 | 28 | 6660 | 64 | 15 |
| 00490b28 |  |  | 00490a48 | 30 | 90 | 84 | 15 |
| 00490ba0 |  | UI widget | 00490000 | 28 | 45 | 48 | 15 |
| 00490c10 |  | UI widget | 00490000 | 28 | 90 | 304 | 15 |
| 00490c80 |  | UI widget | 00490000 | 31 | 0 |  | 0 |
| 00490cfc |  | UI widget | 00490000 | 28 | 360 | 36 | 15 |
| 00490d6c |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00490de0 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00490e54 |  | UI widget | 00490158 | 29 | 90 | 48 | 15 |
| 00490ec8 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00490f3c |  | UI widget | 00490158 | 29 | 225 | 48 | 15 |
| 00490fb0 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491024 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491098 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 0049110c |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491180 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 004911f4 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491268 |  | UI widget | 00490158 | 29 | 90 | 48 | 15 |
| 004912dc |  | UI widget | 00490158 | 29 | 315 | 48 | 15 |
| 00491350 |  | UI widget | 00490158 | 29 | 90 | 48 | 15 |
| 004913c4 |  | UI widget | 00490158 | 29 | 90 | 48 | 15 |
| 00491438 |  | UI widget | 00490158 | 29 | 90 | 48 | 15 |
| 004914ac |  | UI widget | 00490158 | 29 | 90 | 48 | 15 |
| 00491520 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491594 |  | UI widget | 00490158 | 29 | 135 | 48 | 15 |
| 00491608 |  | UI widget | 00490158 | 29 | 180 | 48 | 15 |
| 0049167c |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 004916f0 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491764 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 004917d8 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 0049184c |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 004918c0 |  | UI widget | 00490158 | 29 | 90 | 48 | 15 |
| 00491934 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 004919a8 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491a1c |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491a90 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491b04 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491b78 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491bec |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491c60 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491cd4 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491d48 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491dbc |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491e30 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491ea4 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491f18 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00491f8c |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00492000 |  | UI widget | 00492070 | 28 | 90 | 52 | 15 |
| 00492070 |  | UI widget | 00490000 | 28 | 0 |  | 0 |
| 004920e0 |  | UI widget | 00492070 | 28 | 90 | 52 | 15 |
| 00492150 |  | UI widget | 00492070 | 28 | 90 | 52 | 15 |
| 004921c0 |  | UI widget | 00492070 | 28 | 90 | 52 | 15 |
| 00492230 |  | UI widget | 00492070 | 28 | 90 | 52 | 15 |
| 004922a0 |  | UI widget | 00492070 | 28 | 90 | 52 | 15 |
| 00492310 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00492384 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 004923f8 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 0049246c |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 004924e0 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 00492554 |  | UI widget | 00490158 | 29 | 45 | 48 | 15 |
| 004925c8 |  | UI widget | 00490158 | 29 | 90 | 48 | 15 |
| 0049263c |  | UI widget | 00490158 | 29 | 90 | 48 | 15 |
| 004926b0 |  | UI widget | 00490c80 | 31 | 45 | 56 | 15 |
| 0049272c |  | UI widget | 00490c80 | 31 | 45 | 56 | 15 |
| 004927a8 | FireThrow |  | 0048f63c | 34 | 45 | 264 | 15 |
| 00492830 | VisibleThing |  | 0048f5e0 | 23 | 2440 | 304,pool | 15 |
| 00492c30 |  |  |  | 1 | 45 | 16 | 15 |
| 00492c34 |  |  |  | 1 | 1608 | 16 | 15 |
| 00492c38 | Headtruck | world object kind 6 | 0048fe58 | 75 | 306 | 176,pool | 12 |
| 00492d64 | Johnka | world object kind 10 | 004936c8 | 75 | 6 | 176 | 1 |
| 00492e90 | Tactic |  | 004944c0 | 83 | 6 | 256 | 1 |
| 00492fdc | Tactic |  | 004944c0 | 84 | 9 | 256,pool | 3 |
| 0049312c |  |  | 004940d0 | 4 | 72 | pool | 6 |
| 0049313c | Tactic |  | 0048f2d8 | 83 | 816 | 256,pool | 10 |
| 00493288 | Map |  |  | 3 | 90 | 20 | 15 |
| 00493294 | MapExplorer |  | 004953f8 | 14 | 102 | 40 | 15 |
| 004932cc | Marine | world object kind 0 | 0048fe58 | 75 | 5135 | 176,pool | 15 |
| 004936c8 | VisibleThing |  | 0048f5e0 | 75 | 0 |  | 0 |
| 004937f4 |  | UI widget | 00493878 | 33 | 45 | 4572 | 15 |
| 00493878 |  | UI widget | 00493b50 | 33 | 45 | 4572 | 15 |
| 004938fc |  | UI widget | 00493970 | 29 | 45 | 52 | 15 |
| 00493970 |  | UI widget | 004939e4 | 29 | 45 | 52 | 15 |
| 004939e4 |  | UI widget | 00493a58 | 29 | 45 | 52 | 15 |
| 00493a58 |  | UI widget | 00493acc | 29 | 45 | 52 | 15 |
| 00493acc |  | UI widget | 00493b50 | 33 | 45 | 84 | 15 |
| 00493b50 |  | UI widget | 00493bc4 | 29 | 90 | 52 | 15 |
| 00493bc4 |  | UI widget | 00493c38 | 29 | 45 | 52 | 15 |
| 00493c38 |  | UI widget | 00493cac | 29 | 45 | 52 | 15 |
| 00493cac |  | UI widget | 00493d20 | 29 | 45 | 52 | 15 |
| 00493d20 |  | UI widget | 00493d94 | 29 | 45 | 52 | 15 |
| 00493d94 |  | UI widget | 00493e08 | 29 | 45 | 52 | 15 |
| 00493e08 |  | UI widget | 00493e80 | 30 | 45 | 84 | 15 |
| 00493e80 |  | UI widget | 00490278 | 30 | 45 | 84 | 15 |
| 00493ef8 |  |  |  | 13 | 123 | 24 | 13 |
| 00493f2c |  |  |  | 13 | 83 | 24 | 11 |
| 00493fe0 | VisibleThing |  | 0048f63c | 35 | 180 | 264 | 15 |
| 0049406c | VisibleThing |  | 004904ac | 25 | 107 | 56,pool | 5 |
| 004940d0 |  |  |  | 4 | 8 | 40,pool | 6 |
| 004940e0 | ListTemplate |  |  | 3 | 147 |  | 15 |
| 004940ec | Spider | world object kind 1 | 0048fe58 | 75 | 391 | 176 | 10 |
| 004943a4 | SplashDamage |  | 0048f63c | 35 | 501 | 264,pool | 15 |
| 00494430 | SplashDamage |  | 0048f63c | 34 | 135 | 264,pool | 15 |
| 004944c0 | Tactic |  | 004944f0 | 12 | 2512 | 40 | 15 |
| 004944f0 | Tactic |  |  | 83 | 1795 | 256,pool | 15 |
| 0049463c |  |  |  | 2 | 220 | 2072,pool | 13 |
| 00494644 |  |  |  | 1 | 0 |  | 0 |
| 00494648 | Tactic |  | 004947a8 | 88 | 436 | 440,pool | 15 |
| 004947a8 | Town |  | 004947fc | 21 | 432 | 440,pool | 15 |
| 004947fc | Town |  | 004944c0 | 12 | 432 | 440,pool | 15 |
| 00494840 | Triggers |  | 004907c0 | 14 | 0 |  | 0 |
| 00494878 | Triggers |  | 00494840 | 14 | 276 | pool | 15 |
| 004948b0 |  |  | 00494840 | 14 | 1608 | 40,pool | 15 |
| 004948e8 | Truck | world object kind 4 | 0048fe58 | 75 | 21 | 176 | 2 |
| 00494a14 |  | UI widget | 00490070 | 29 | 0 |  | 0 |
| 00494f10 | VisibleThing |  | 0048f63c | 34 | 90 | 264,pool | 15 |
| 00494f98 | VisibleThing |  | 0048f63c | 34 | 45 | 264 | 15 |
| 00495020 | VisibleThing |  | 0048f63c | 34 | 45 | 264 | 15 |
| 004950a8 | ListTemplate |  | 0048f63c | 34 | 90 | 264 | 15 |
| 00495130 | VisibleThing |  | 0048f63c | 34 | 405 | 264,pool | 15 |
| 004951b8 | Zeppelin | world object kind 3 | 004936c8 | 75 | 21 | 176 | 5 |
| 00495318 | Bitmap |  | 00495330 | 6 | 0 |  | 0 |
| 00495330 | Bitmap |  | 004955c0 | 24 | 0 |  | 0 |
| 004953e8 | Data |  |  | 4 | 223 | 32 | 15 |
| 004953f8 | Data |  | 004953e8 | 12 | 3283 | 40 | 15 |
| 00495428 | Data |  | 004953f8 | 18 | 0 |  | 0 |
| 00495470 |  |  |  | 3 | 0 |  | 0 |
| 0049547c | lib |  |  | 1 | 450 | pool | 15 |
| 00495480 | color |  |  | 1 | 45 | 72 | 15 |
| 00495528 |  |  | 004955c0 | 17 | 225 | 32 | 15 |
| 0049556c |  |  |  | 1 | 45 | 200 | 15 |
| 00495570 |  |  |  | 1 | 45 | 24 | 15 |
| 00495574 |  |  | 00495570 | 1 | 45 | 32 | 15 |
| 00495578 |  |  | 00495570 | 1 | 45 | 28 | 15 |
| 0049557c |  |  | 00495570 | 1 | 147 | 24 | 15 |
| 00495580 | Lvmaps |  | 00495610 | 16 | 45 | 32 | 15 |
| 004955c0 |  |  |  | 16 | 0 |  | 0 |
| 00495610 | page |  | 004955c0 | 16 | 45 | 24 | 15 |
| 00495654 |  |  |  | 1 | 0 |  | 0 |

## Pliki źródłowe oryginału (z napisów assert w pliku wykonywalnym)

3DSound.cpp, ASE.cpp, Actions.cpp, Anim.cpp, Arrow.cpp, Axe.cpp, BallExpl.cpp, Bird.cpp, Bitmap.cpp, Boat.cpp, BuildTree.cpp, Building.cpp, Cannon.cpp, Conditions.cpp, Data.cpp, Decoration.cpp, Defence.cpp, Double.cpp, FireThrow.cpp, Harvest.cpp, Headtruck.cpp, Heapsort.cpp, Johnka.cpp, ListTemplate.hpp, LoadGame.cpp, Lvmaps.cpp, Map.cpp, MapExplorer.cpp, Marine.cpp, Mouse.cpp, Side.cpp, Spider.cpp, SplashDamage.cpp, Sprite.hpp, Submarine.hpp, Tactic.cpp, Test.cpp, Town.cpp, TownsInfo.cpp, Triggers.cpp, Truck.cpp, View.cpp, VisibleThing.cpp, VisibleThing.hpp, WaveSearch.cpp, Weapon.cpp, Zeppelin.cpp, color.cpp, display.cpp, err.cpp, lib.cpp, loadmap.hpp, locman.cpp, mfctemplates.h, mix.cpp, page.cpp, snd.cpp, sprite.cpp

## Funkcje zmapowane na pliki źródłowe

253 funkcji ma przypisany plik (po napisach assert). Pełna lista: `work/source_map.json` (generowana lokalnie).

# Strategizer2D

Rundenbasiertes 2D-Strategiespiel in C++17 (SFML 2), inspiriert von *Noobs in Combat*.
Du spielst auf einem 12×12-Feld gegen eine KI, die ihre Truppen schon platziert hat. Du stellst deine
Truppen im Rahmen eines Budgets selbst auf und kämpfst dann Runde für Runde.

## Bauen & starten

Benötigt: C++17-Compiler, CMake ≥ 3.16, SFML 2.5/2.6 (wird sonst automatisch heruntergeladen).

```bash
# Linux (Debian/Ubuntu): sudo apt install libsfml-dev cmake g++
cmake -S . -B build
cmake --build build -j
./build/Strategizer2D        # aus dem Projektordner starten
ctest --test-dir build       # Logik-Tests (ohne Grafik)
```

Das Spiel findet `data/` und `assets/` automatisch (aktueller Ordner, Ordner der .exe, Projektordner).

### Windows

1. Einmalig installieren: [CMake](https://cmake.org/download) (Haken „Add to PATH“), [Git](https://git-scm.com)
   und einen C++-Compiler (Visual Studio Build Tools oder MinGW-w64).
2. `build.bat` doppelklicken – lädt SFML automatisch und baut das Spiel (nur beim ersten Mal / nach Code-Änderungen nötig).
3. `play.bat` doppelklicken – startet das Spiel (baut vorher automatisch, falls noch nicht geschehen).

Die Version (oben links, halbtransparent) steht in `CMakeLists.txt` bei `project(... VERSION x.y.z)`.

## Bedienung

| Aktion | Eingabe |
|---|---|
| Aufstellung: Truppe wählen → Feld in der blauen Zone klicken | Linksklick |
| Aufgestellte Truppe entfernen (Budget kommt zurück) | Rechtsklick |
| Schlacht beginnen / Zug beenden | Leertaste oder Button |
| Truppe wählen | Linksklick (Tab = nächste Truppe) |
| Bewegen | Truppe wählen, Klick auf ein **blaues** Feld |
| Waffe wählen (zeigt ihre Reichweite) | Klick auf die Waffe oder Taste 1–5 (nochmal drücken = abwählen) |
| Waffe abwählen | Q (oder Esc) |
| Angreifen | Waffe wählen, dann Klick auf ein **rot** markiertes Ziel |
| Auswahl aufheben | Esc / Rechtsklick |

Standardmäßig ist keine Waffe gewählt: du siehst nur die Bewegungsreichweite (blau). Wählst du eine Waffe, wird stattdessen
ihre Reichweite angezeigt (orange). Beim Überfahren eines Ziels siehst du den Schaden vorab.

## Regeln

* Jede Truppe darf pro Runde **einmal bewegen UND einmal angreifen** (in beliebiger Reihenfolge;
  `double_attack` = zweimal angreifen).
* **Nebel des Krieges:** Du siehst nur Felder/Gegner innerhalb der **Sicht** (`vision`, Manhattan-Distanz) deiner
  Truppen. Infanterie sieht standardmäßig weiter als Fahrzeuge. Gegner im Nebel kannst du nicht angreifen – die KI
  unterliegt demselben Nebel.
* **Hinterhalte:** Einige Gegnergruppen sind zu Beginn nicht auf der Karte. Sobald eine eigene Truppe nach einer
  Bewegung auf einem Auslöser-Feld der Gruppe steht, erscheint die ganze Gruppe (ein Feld reicht).
* **Willenskraft:** Treffer senken sie (`will_damage`). Bei 0 kann sich die Truppe in ihrem nächsten Zug
  nicht bewegen. Nach diesem Zug ist sie wieder auf dem Standardwert.
* **Waffen-Abklingzeit** (`cooldown`): so viele eigene Züge ist die Waffe nach dem Schuss gesperrt.
* **Gebäude** (★ = Missionsziel) kann nur **Infanterie** einnehmen (auf das Feld ziehen genügt) – man kann sie aber auch
  **zerstören**: jede Waffe kann Gebäude beschießen (Waffe wählen, Gebäude anklicken). Ein zerstörtes Gebäude zählt als
  erledigtes Ziel, so ist ein Sieg auch ohne Infanterie möglich. HP/Rüstung pro Gebäude stehen in der Karte.
* **Treibstoff** (`fuel`): Fahrzeuge verbrauchen 1 Treibstoff in jedem Zug, in dem sie sich bewegen; bei 0 stehen sie still.
  Der Supply Truck hat keinen Treibstoff-Bedarf.
* **Heilung & Versorgung** (grün angezeigt, nur für eigene Truppen, nur wenn nötig):
  `heal` = Infanterie heilen (Medic, Ambulance), `repair` = Fahrzeuge reparieren, `refuel` = Treibstoff auffüllen,
  `supply` = beides (Supply Truck). Das Attribut `heilung(10)` heilt die Truppe selbst um 10 HP zu Beginn jedes eigenen
  Zuges (Medic; die Ambulance hat es nicht).
* **Status-Icons** oben rechts im Feld einer Truppe: ! Moral gebrochen, F Treibstoff niedrig/leer, grünes + Heiler/Selbstheilung,
  T Tarnung, R Funk, 2 Doppelangriff.
* **Biome** verändern Bewegungskosten und erhaltenen Schaden (Plains = Standard).
* **Tarnung** (`camouflage`): Truppe ist nur sichtbar, wenn eine gegnerische Truppe höchstens 2 Felder entfernt ist (und in deren Sicht).
* **Funk** (`radio`): Waffen mit `needs_radio=1` (z. B. spätere Artillerie) funktionieren nur, solange eine
  eigene Truppe mit Funkgerät lebt.
* Sieg: Ziel der Mission erfüllt oder alle Gegner vernichtet. Niederlage: alle eigenen Truppen verloren oder
  Rundenlimit (`max_turns`) überschritten.

### Schadensformeln

```
Rüstung == 0:  Schaden = Damage * TerrainMultiplier
Rüstung  > 0:  Schaden = ((Damage * AP) / Rüstung) * TerrainMultiplier
```

(auf ganze Zahlen gerundet, mindestens 1; `TerrainMultiplier` ist der Wert des Feldes, auf dem der Verteidiger steht).

## Daten bearbeiten (alles in `data/`)

Alle CSV-Dateien lassen sich mit Excel/LibreOffice oder einem Texteditor bearbeiten (`,` oder `;` als Trenner,
Zeilen mit `#` sind Kommentare). Fehler (Tippfehler, unbekannte IDs) werden beim Start mit Datei und Zeile gemeldet.

| Datei | Inhalt |
|---|---|
| `units.csv` | Truppen: HP, Willenskraft, Rüstung, Bewegung, Sicht, Treibstoff, Kosten, Attribute, bis zu 5 Waffen (`weapon1`–`weapon5`) |
| `weapons.csv` | Waffen: Reichweite, Schaden, AP, Willens-Schaden, Cooldown, `needs_radio`, `type` (damage/heal/repair/refuel/supply) |
| `terrain.csv` | Biome: Farbe, Bewegungskosten (Infanterie/Fahrzeug, 0 = unpassierbar), Schadensmultiplikator |
| `campaigns.csv` | Kampagnen |
| `missions.csv` | Missionen je Kampagne: Ziel (`capture`/`eliminate`), Karte, Budget, Rundenlimit |
| `maps/*.map` | Karten (immer 12×12, Größe in `src/Config.hpp`) |

Eine **neue Truppe** = eine neue Zeile in `units.csv` (Waffen aus `weapons.csv` referenzieren).
Eine **neue Mission** = Zeile in `missions.csv` + neue `.map`-Datei.

### Kartenformat

```
[tiles]        12 Zeilen à 12 Zeichen: P Plains, F Forest, H Hills, M Mountain, R River (Symbole aus terrain.csv)
[deploy]       Aufstellungszone als Rechteck: x0,y0,x1,y1  (mehrere Zeilen möglich)
[buildings]    Name,x,y,Besitzer(enemy|player|neutral),Ziel(1|0)[,HP[,Rüstung]]
[triggers]     Gruppe,x0,y0,x1,y1   Auslöser-Rechteck (mehrere Zeilen pro Gruppe möglich)
[enemies]      unit_id,x,y,Verhalten(aggressive|hold)[,Gruppe]   hold = bewegt sich nie, schießt nur;
               mit Gruppe: erscheint erst, wenn die Gruppe ausgelöst wird
```
Koordinaten: x = Spalte, y = Zeile, jeweils ab 0 (oben links).

## Bilder

Truppenbilder liegen in `assets/units/` (PNG mit transparentem Hintergrund, quadratisch, empfohlen 128×128 oder 256×256; werden automatisch auf Feldgröße skaliert). Dateiname = Spalte
`sprite` in `units.csv` (Standard `<id>.png`, also `rifleman.png`, `trooper.png`, `light_tank.png`,
`medium_tank.png`, `heavy_tank.png`, `medic.png`, `ambulance.png`, `truck.png`). Fehlt ein Bild, wird eine einfache Ersatzgrafik gezeichnet –
das Spiel ist also auch ohne Bilder spielbar. Schrift: DejaVu Sans (freie Lizenz, `assets/fonts/`).

## Projektstruktur

```
src/Battle.*       Spiellogik (Karte, Aufstellung, Bewegung, Kampf, Runden, KI) – ohne Grafik
src/Data.*, Csv.*  CSV-Daten laden und prüfen
src/App.*, Screens.*, BattleScreen.*   SFML-Oberfläche
tests/             Logik-Tests (Formeln, Daten, Regeln, KI-Simulationen)
```

`./build/Strategizer2D --screenshots <ordner>` spielt eine kleine Demo ab und speichert Screenshots
(nützlich für automatische Checks).

# Strategizer2D

Rundenbasiertes 2D-Strategiespiel in C++17 (SFML 2), inspiriert von *Noobs in Combat*.
Du spielst auf einem Spielfeld (z. B. 12×12) gegen eine KI, die ihre Truppen schon platziert hat. Du stellst deine
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
* **Gebäude** blockieren ihr Feld – **niemand kann auf einem Gebäude stehen**. Infanterie, die **neben** einem Gebäude
  steht und sich noch bewegen kann, nimmt es per **Klick auf das Gebäude** ein (verbraucht die Bewegung). Man kann Gebäude
  auch **zerstören**: jede Waffe kann sie beschießen (Waffe wählen, Gebäude anklicken). Ein zerstörtes Ziel-Gebäude
  zählt als erledigt, so ist ein Sieg auch ohne Infanterie möglich. Gebäudetypen (HP, Rüstung, Einkommen, Aktionen) stehen
  in `data/buildings.csv`: **Outpost** (Truppen kaufen), **Supply Post** (unbegrenzt heilen/reparieren/versorgen), **Bunker**.
* **Munition:** Jede Waffe hat eine begrenzte Anzahl Schüsse (`ammo` in `weapons.csv`, pro Truppe überschreibbar mit
  `ammo1`–`ammo5` in `units.csv`; z. B. Rifleman 3, Trooper 4). `0` oder leer = unbegrenzt. Leere Waffen sind gesperrt, bis
  ein **Supply** (Supply Truck) die Truppe auffüllt: *Supply* füllt **alle** Munition und den Treibstoff auf. Der Supply
  ist selbst begrenzt (5 Einsätze). Versorgt ein Supply Truck einen anderen Supply Truck, wird addiert/subtrahiert
  (der Geber gibt `damage` Einsätze ab, der Empfänger bekommt sie) statt komplett aufzufüllen. Supply Truck: 3 Aktionen pro
  Runde (Supply, Repair für Fahrzeuge, Light Heal für Infanterie).
* **Geld:** Übrig gebliebenes Aufstellungs-Budget wird zu Geld. Es gibt Geld für jeden Abschuss (`kill_reward` in
  `data/rules.csv`, Standard 5) und Einkommen für jedes eigene Gebäude, wenn du deinen Zug beendest (Outpost 10,
  Supply Post 5, Bunker 5). Eigene Außenposten anklicken → **Truppen kaufen** (Taste B): die neue Truppe erscheint neben
  dem Gebäude und kann ab der nächsten Runde handeln (1 Kauf pro Gebäude und Runde).
* **Ziele:** `capture` (Gebäude einnehmen/zerstören), `eliminate` (alle Gegner vernichten), `survive` (so viele Runden
  durchhalten, wie `max_turns` angibt). Gebäude mit der Rolle `protect` darfst du nie verlieren (zerstört oder vom
  Gegner eingenommen = Niederlage). Die linke Seitenleiste zeigt Rundenzähler, Geld und Ziele.
* **Treibstoff** (`fuel`): Fahrzeuge verbrauchen 1 Treibstoff in jedem Zug, in dem sie sich bewegen; bei 0 stehen sie still.
  Der Supply Truck hat keinen Treibstoff-Bedarf.
* **Heilung & Versorgung** (grün angezeigt, nur für eigene Truppen, nur wenn nötig):
  `heal` = Infanterie heilen (Medic, Ambulance), `repair` = Fahrzeuge reparieren, `refuel` = Treibstoff auffüllen,
  `supply` = beides (Supply Truck). Das Attribut `heilung(10)` heilt die Truppe selbst um 10 HP zu Beginn jedes eigenen
  Zuges (Medic; die Ambulance hat es nicht).
* **Aktions-Icons** unten rechts im Feld deiner Truppen (nur in deinem Zug): blauer Pfeil = kann sich noch bewegen, orangenes Fadenkreuz = kann noch angreifen, grünes Plus = kann noch heilen/versorgen, rotes durchgestrichenes **M** = Munition leer, rotes durchgestrichenes **F** = Treibstoff leer. Ist eine Aktion verbraucht, verschwindet ihr Icon.
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
| `units.csv` | Truppen: HP, Willenskraft, Rüstung, Bewegung, Sicht, Treibstoff, Kosten, Attribute, Aktionen pro Runde (`attacks`), bis zu 5 Waffen (`weapon1`–`weapon5`) mit optionaler Munition (`ammo1`–`ammo5`) |
| `weapons.csv` | Waffen: Reichweite, Schaden, AP, Willens-Schaden, Cooldown, `needs_radio`, `type` (damage/heal/repair/refuel/supply), `ammo` |
| `buildings.csv` | Gebäudetypen: HP, Rüstung, Einkommen, Käufe pro Runde, eigene Waffen/Aktionen |
| `rules.csv` | Globale Regeln (`kill_reward`) |
| `terrain.csv` | Biome: Farbe, Bewegungskosten (Infanterie/Fahrzeug, 0 = unpassierbar), Schadensmultiplikator |
| `campaigns.csv` | Kampagnen |
| `missions.csv` | Missionen je Kampagne: Ziel (`capture`/`eliminate`), Karte, Budget, Rundenlimit |
| `maps/*.map` | Karten – beliebige Größe von 4×4 bis 32×32 (Rechteck, die Größe ergibt sich aus den `[tiles]`-Zeilen; das Spielfeld skaliert automatisch) |

Eine **neue Truppe** = eine neue Zeile in `units.csv` (Waffen aus `weapons.csv` referenzieren).
Eine **neue Mission** = Zeile in `missions.csv` + neue `.map`-Datei.

### Kartenformat

```
[tiles]        Zeilen mit je einem Zeichen pro Feld (alle Zeilen gleich lang), z. B. P Plains, F Forest, H Hills,
               M Mountain, R River – die Symbole stammen aus terrain.csv, jede Karte nutzt nur die, die sie braucht
[deploy]       Aufstellungszone als Rechteck: x0,y0,x1,y1 = Ecke oben links und Ecke unten rechts (mehrere Zeilen möglich)
[buildings]    Typ,x,y,Besitzer(enemy|player|neutral),Rolle(capture|protect|none)[,HP[,Rüstung[,Name]]]   Typ aus buildings.csv
[triggers]     Gruppe,x0,y0,x1,y1   Auslöser-Rechteck (mehrere Zeilen pro Gruppe möglich)
[enemies]      unit_id,x,y,Verhalten(aggressive|hold)[,Gruppe]   hold = bewegt sich nie, schießt nur;
               mit Gruppe: erscheint erst, wenn die Gruppe ausgelöst wird
```
Koordinaten: x = Spalte von links, y = Zeile von oben, jeweils ab 0 – (0,0) ist das Feld oben links, bei 12×12 ist (11,11) unten rechts.

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

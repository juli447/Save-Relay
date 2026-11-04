# SaveRelay

Lokaler Save Transfer zwischen 2 3DS Systemen

## Download und Installation

- **CIA:** `SaveRelay.cia` mit FBI installieren und anschließend im HOME-Menü starten.
- **3DSX:** `SaveRelay.3dsx` nach `/3ds/SaveRelay/SaveRelay.3dsx` kopieren und im Rosalina Homebrew Launcher starten. `SaveRelay.smdh` kann daneben liegen.
- Lokale Kommunikation auf beiden Konsolen einschalten.
- Das Zielspiel muss installiert bzw. als Spielkarte eingelegt sein und einen initialisierten Spielstand besitzen. Vorher Spiel schließen.

## Bedienung

| Eingabe | Funktion |
| --- | --- |
| Steuerkreuz / Touch | Spiel im Icon-Raster auswählen |
| L / R | Seite wechseln |
| A | Spielstand sichern und senden |
| Y | Sender suchen, auswählen und Empfang bestätigen |
| X | Letzte Sicherheitskopie dieses Spiels zurückspielen |
| SELECT | Spieleliste neu laden |
| START | Beenden |
| B innerhalb eines Vorgangs | Zurück / Abbrechen, soweit noch möglich |

Nach der Empfangsbestätigung erfolgen Empfang, Archivprüfung, Sicherheitskopie, Restore und Rücklesen automatisch. Während des Restores ist kein Abbruch möglich.

Backups liegen unter `/3ds/SaveRelay/<Titel-ID>-<Medium>/`. `before-receive-*.srb` enthält den Save vor Empfang; `recovery.txt` verweist auf die letzte solche Sicherung. `.srb` ist ein eigenes Format und nicht mit Checkpoint kompatibel.

## Umfang und Grenzen

Normale 3DS-Benutzer-Spielstände auf SD und Spielkarte. Sender und Empfänger brauchen exakt dieselbe Titel-ID und passende Spiel-/Update-Versionen.

Maximal 256 MiB pro Archiv, 4096 Einträge und 16 Verzeichnisebenen.

Vor dem ersten Ziel-Schreibzugriff werden das komplette Archiv und seine CRC geprüft und der bisherige Save gesichert. Ein fehlgeschlagener Restore versucht einen Rollback.

## Selbst bauen

Voraussetzungen: [devkitPro/devkitARM](https://devkitpro.org/wiki/Getting_Started), `libctru`, `3dsxtool`, `smdhtool`, GNU Make. Für CIA zusätzlich [makerom](https://github.com/3DSGuy/Project_CTR) und [bannertool](https://github.com/Epicpkmn11/bannertool).

Unter Linux x86_64 / WSL2:

```bash
# Nach Installation der offiziellen devkitPro-3DS-Toolchain:
export DEVKITPRO=/opt/devkitpro
export DEVKITARM="$DEVKITPRO/devkitARM"
bash scripts/bootstrap-tools.sh
export PATH="$PWD/.tools/bin:$PATH"
make -j2
```

Der Helper lädt festgelegte Tool-Versionen, prüft die SHA-256-Werte und schreibt nur nach `.tools/` im Projekt. Für andere Systeme makerom/bannertool selbst bereitstellen und in PATH aufnehmen oder übergeben:

```bash
make -j2 MAKEROM=/pfad/zu/makerom BANNERTOOL=/pfad/zu/bannertool
```

Ergebnisse: `SaveRelay.cia`, `SaveRelay.3dsx`, `SaveRelay.smdh`. Nur Homebrew Launcher: `make 3dsx`. Nur Installation: `make cia`. Aufräumen: `make clean`.

Die Quell-PNGs und die stille Banner-WAV liegen unter `assets/`. `python3 scripts/generate_assets.py` erzeugt diese eigenen Grafiken erneut. Es werden keine Spiele-Icons oder Nintendo-Grafiken mitgeliefert; Spiel-Icons werden auf der Konsole aus den Metadaten gelesen.

## Tests und CIA-Metadaten

```bash
bash tests/run.sh
# In einer ptrace-basierten Umgebung bei LeakSanitizer-Problemen:
ASAN_OPTIONS=detect_leaks=0 bash tests/run.sh
```

Tests prüfen Archivvalidierung, Backups, Restore/Read-back/Rollback, wiederholte Recovery-Verweise, Icon-Decodierung, Navigation und das Double-Buffer-Presenting.

CIA Title-ID: **`000400000F7A1200`**, Product Code: `CTR-HB-SVRL`, die App filtert sich selbst aus der Spieleauswahl.

Die CIA benötigt `fs:USER`, AM, UDS und die üblichen Grafik-/Eingabedienste. Die RSF aktiviert Dateisystem-Werkzeugrechte für fremde Benutzer-Saves; es werden nur normale 3DS-Anwendungstitel auf SD/Spielkarte geöffnet.

## Lizenz und Herkunft

Eigener SaveRelay-Code und eigene Assets: MIT, siehe [LICENSE](LICENSE). Dritte Komponenten und Werkzeuge: [THIRD_PARTY.md](THIRD_PARTY.md).

# Validierung

- Grundlage: SaveRelay 0.4.1 aus dem vorhandenen Release-Archiv. Save-, Funk-, UI- und Display-Code entsprechen diesem Stand; main.cpp filtert zusaetzlich den eigenen CIA-Titel aus.
- Kein Extdata-Code in source/ oder include/.
- Linux-Bootstrap mit festgelegten ZIP-SHA256-Werten erfolgreich ausgefuehrt.
- Frischer Cross-Build mit devkitARM/libctru erzeugt 3DSX, SMDH und CIA ohne Compilerwarnungen.
- Vier Host-Testgruppen (ASan/UBSan) bestanden; LeakSanitizer in der ptrace-Umgebung deaktiviert.
- CIA mit ctrtool 1.3.0 geoeffnet und geprueft: Inhaltshash, Exheader, ExeFS sowie Code-, Icon-, Banner- und Logo-Hashes GOOD.
- Title-ID 000400000F7A1200, Installations-Version 1, Ext-Savedata-ID 0.
- Ticket/TMD/AccessDescriptor sind Homebrew-testsigniert und bestehen keine Nintendo-Signaturpruefung. CFW ist Voraussetzung.
- GitHub-Workflow vorhanden und gegen die Container-Werkzeuge vorbereitet, aber nicht auf einem GitHub-Runner ausgefuehrt.
- CIA-Installation, Start im HOME-Menue und Save-Zugriffe noch nicht auf Hardware getestet.

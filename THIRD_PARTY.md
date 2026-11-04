# Herkunft und Drittkomponenten

- SaveRelay-Anwendung und eigene Assets: GPL-3.0-or-later, siehe LICENSE.
- libctru wird beim Cross-Build statisch eingebunden: https://github.com/devkitPro/libctru. Die zugehörigen Lizenzbedingungen stehen im README des Projekts und sind unten wiedergegeben.
- devkitARM/GCC, newlib und C++-Runtime: Bestandteil der separat installierten devkitPro-Toolchain; jeweilige Upstream-Lizenzen und Runtime-Ausnahmen gelten. Compiler werden nicht mitgeliefert.
- makerom 0.18.3 und bannertool aus Release v1.2.2 werden vom Bootstrap-Skript unverändert heruntergeladen und im ignorierten Ordner `.tools/` gespeichert. Sie sind Build-Werkzeuge und kein Teil des veröffentlichten SaveRelay-Quellcodes. URLs und Archiv-Prüfsummen stehen im Skript.
- Die frühere allgemeine Makefile-Vorlage stammte aus devkitPro/3ds-examples. Die CIA-Ausgabe verwendet ein eigenes Makefile.
- Checkpoint diente als Referenz für Save-Archivzugriffe und als UX-Vorbild. Es werden keine Checkpoint-Quelldateien oder Grafiken mitgeliefert.

## libctru-Lizenz (aus dem Upstream-README)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you
   must not claim that you wrote the original software. If you use
   this software in a product, an acknowledgment in the product
   documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and
   must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source
   distribution.

# Room Corrector by Uncertain

Plugin VST3 + Standalone (Windows 64-bit) — correcteur de voix "ingé son invisible".

## Compatibilité
- VST3 : FL Studio, Mixcraft 9+, Ableton 10.1+, Reaper, Studio One, Cubase, Bitwig, Cakewalk...
- Standalone : fonctionne sans DAW
- Pro Tools (AAX) : non supporte

## Compilation
Automatique via GitHub Actions (onglet Actions). Produit 3 artifacts :
- RoomCorrector-Windows : le .vst3 + le standalone .exe
- RoomCorrector-Installer : RoomCorrector-Setup.exe (installe le VST3 tout seul)
- BUILD-LOGS : logs de diagnostic

## Structure
- .github/workflows/build.yml : compilation + installateur Inno Setup
- CMakeLists.txt : configuration JUCE 8 (FetchContent)
- DSP.h : moteur audio (anti-pompage, air adaptatif, true-peak)
- PluginProcessor / PluginEditor : coeur + interface

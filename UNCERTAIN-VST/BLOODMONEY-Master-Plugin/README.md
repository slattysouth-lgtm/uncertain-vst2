# BLOODMONEY // Master Engine — VST3

Plugin de mastering (EQ → compression → saturation 4× → **limiteur look-ahead brick-wall**).
Même chaîne et même limiteur anti-grésillement que la version web, porté en C++/JUCE pour tourner
en temps réel dans un DAW.

- **Mixcraft** ✅ (VST3, natif 64-bit)
- **FL Studio** ✅ (VST3)
- **Pro Tools** ⚠️ AAX uniquement — voir la section dédiée en bas.

---

## 1. Outils à installer (gratuits, une fois)

1. **Visual Studio 2022 Community** — coche la charge de travail *« Développement Desktop en C++ »*
   (fournit le compilateur MSVC + CMake).
2. **Git** — https://git-scm.com (sert à télécharger JUCE automatiquement).

Rien d'autre : JUCE est récupéré tout seul par CMake au premier build (il faut juste une connexion).

## 2. Compiler le VST3 (2 commandes)

Ouvre *« x64 Native Tools Command Prompt for VS 2022 »*, place-toi dans ce dossier, puis :

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Le premier `cmake -B build` télécharge JUCE (quelques minutes la 1re fois). Le second compile.

## 3. Où est le plugin

Grâce à `COPY_PLUGIN_AFTER_BUILD`, le `.vst3` est copié automatiquement dans :

```
C:\Program Files\Common Files\VST3\BLOODMONEY Master.vst3
```

Si la copie échoue (droits admin), récupère-le manuellement ici :

```
build\BLOODMONEYMaster_artefacts\Release\VST3\BLOODMONEY Master.vst3
```

et copie-le toi-même dans `C:\Program Files\Common Files\VST3\`.

Une version **Standalone** (appli autonome) est aussi générée dans
`...\Release\Standalone\` — pratique pour tester sans DAW.

## 4. Charger dans le DAW

- **FL Studio** : *Options → Manage plugins → Find installed plugins* (Find more plugins), puis cherche « BLOODMONEY ».
- **Mixcraft** : *Preferences → Plug-Ins*, vérifie que `C:\Program Files\Common Files\VST3` est dans la liste, puis *Rescan*. Tu peux aussi glisser le `.vst3` directement dans la fenêtre Mixcraft.

Pose-le sur le **bus master**, en **dernier** de la chaîne.

---

## 5. Pro Tools / AAX (optionnel, lourd)

Pro Tools ne lit **aucun VST**, seulement l'**AAX**, et un AAX doit être **signé PACE** pour
charger dans un Pro Tools commercial (un AAX non signé ne se charge que dans la version
spéciale *Pro Tools Developer*). Le code est prêt ; il manque juste le SDK + la signature :

1. S'inscrire au **Avid Developer Program** et récupérer le **AAX SDK**.
2. Build avec la cible AAX activée :
   ```bat
   cmake -B build -G "Visual Studio 17 2022" -A x64 -DBM_BUILD_AAX=ON -DAAX_SDK_PATH=C:/chemin/aax-sdk
   cmake --build build --config Release
   ```
3. Obtenir un **iLok physique** + le toolkit **PACE Eden**, puis signer le `.aax` avec `wraptool`
   avant de le poser dans `C:\Program Files\Common Files\Avid\Audio\Plug-Ins\`.

Pour un usage perso, c'est beaucoup d'administratif : le mastering étant la dernière étape,
le plus simple reste d'exporter ton mix de Pro Tools et de le passer dans le VST3 (Mixcraft/FL)
ou la version web.

---

## Réglages

7 potards + 3 presets (PROPRE / STREAM / PUISSANCE). Réglages identiques à l'outil web.
Le **Plafond** fixe la limite absolue de sortie ; le limiteur garantit qu'aucun échantillon
ne la dépasse — d'où le « gros sans grésillement ».

> Rappel : Spotify/Apple normalisent à −14 LUFS. Vise la densité propre, pas le chiffre maximal.

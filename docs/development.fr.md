# Développement

## Structure du projet {#project-structure}

```
RLTMBridge.sln
RLTMBridge/
├── RLTMBridge.h / .cpp    Classe du plugin, chargement / déchargement et hooks du jeu
├── Settings/              Cvars, commandes de la console et fenêtre de réglages
├── WebSocket/             Connexion au serveur, messages, commandes du serveur et moniteur local
├── Protocol/              Format des messages, sans le SDK BakkesMod (testé unitairement)
├── Game/                  Données du jeu, statistiques, événements, résultat et actions (HUD, spectateur, replays)
├── IMGUI/, fmt/           Bibliothèques tierces
└── version.h              Version du plugin
tests/                     Tests unitaires du format des messages
tools/test_server.py       Serveur de test
docs/                      Cette documentation (MkDocs)
```

Le plugin est une seule classe, `RLTMBridge`, déclarée dans `RLTMBridge.h` et implémentée dans plusieurs fichiers sources par catégorie. Le jeu n'est accédé que depuis le thread du jeu : le thread réseau de la bibliothèque WebSocket ne fait que mettre en file d'attente les commandes du serveur, exécutées à l'image suivante.

## Compilation {#build}

Prérequis : Windows, Visual Studio 2022 avec la charge de travail C++, [vcpkg](https://vcpkg.io) intégré à Visual Studio, et BakkesMod installé.

1. Ouvrir `RLTMBridge.sln`.
2. Compiler la configuration `Release | x64`.

vcpkg installe les dépendances (IXWebSocket avec TLS, nlohmann/json) depuis `vcpkg.json`, et le SDK BakkesMod est trouvé depuis l'installation de BakkesMod (`BakkesMod.props`). La DLL est générée dans `plugins/` et adaptée pour BakkesMod par l'étape post-build.

## Tests

Le format des messages (`RLTMBridge/Protocol`) ne dépend pas du SDK BakkesMod et est testé unitairement sur n'importe quelle plateforme :

```bash
sh tests/run.sh   # nécessite nlohmann/json, par exemple apt install nlohmann-json3-dev
```

Le comportement en jeu est testé manuellement avec la checklist du dépôt (`docs/TEST-CHECKLIST.md`) et le serveur de test (`tools/test_server.py`).

## Intégration continue {#continuous-integration}

Le workflow **Build** tourne à chaque push et pull request :

- `test` : tests unitaires sous Linux,
- `build` : compilation MSVC avec vcpkg et le SDK BakkesMod, la DLL est publiée dans l'artefact `RLTMBridge`,
- `release` : sur un tag `v*`, crée une release GitHub avec la DLL.

Le numéro de build de la version (`VERSION_BUILD` dans `version.h`) est le numéro du run de la CI. Les compilations locales gardent celui de `version.h`.

Le workflow **Docs** construit cette documentation à chaque push et pull request, et la publie sur GitHub Pages depuis la branche `main`.

## Documentation

La documentation est écrite en Markdown dans `docs/` et construite avec [MkDocs Material](https://squidfunk.github.io/mkdocs-material/). Chaque page existe en anglais (`page.md`) et en français (`page.fr.md`).

```bash
pip install -r docs/requirements.txt
mkdocs serve   # http://127.0.0.1:8000
```

## Modifier le protocole {#protocol-changes}

En modifiant les messages :

- ajoutez des champs et des topics sans casser les existants quand c'est possible,
- sinon, augmentez `protocol_version` dans `RLTMBridge.h`,
- mettez à jour la [documentation du protocole](protocol/index.md), l'[historique des versions](changelog.md) et les tests unitaires.

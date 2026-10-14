# Démarrage

## Prérequis

- Rocket League sous Windows, avec [BakkesMod](https://bakkesmod.com) installé et lancé.
- Un serveur WebSocket qui reçoit les données : RLTM, ou le serveur de test du dépôt pour essayer le plugin (voir [Essayer sans serveur](#try-it-without-a-server)).

## Installation

1. Télécharger `RLTMBridge.dll` :
    - depuis les [releases](https://github.com/Minarox/RLTM-Bridge/releases) du dépôt,
    - ou depuis les artefacts du dernier run [Build](https://github.com/Minarox/RLTM-Bridge/actions/workflows/build.yml) (artefact `RLTMBridge`).
2. La copier dans le dossier `plugins` de BakkesMod : fenêtre BakkesMod → *File* → *Open BakkesMod folder* → `plugins/`.
3. Dans Rocket League, ouvrir la console BakkesMod (++f6++) et taper :

    ```
    plugin load rltmbridge
    ```

    La console affiche `RLTM Bridge v1.x.x.x loaded`.

4. Pour le charger automatiquement à chaque lancement, l'activer dans le gestionnaire de plugins : ++f2++ → *Plugins* → *Plugin Manager*.

## Se connecter au serveur

Ouvrir les réglages du plugin : ++f2++ → *Plugins* → *RLTM Bridge*.

1. **Server URL** : adresse du serveur WebSocket, `ws://` ou `wss://` (TLS).

    === "RLTM"

        ```
        ws://<adresse de RLTM>:<port>/?token=<jeton du tournoi>
        ```

        Le jeton du tournoi indique à quel tournoi appartient le match.

    === "Serveur local"

        ```
        ws://localhost:3000
        ```

2. **Client name** (facultatif) : nom de cet ordinateur pour le serveur, pour distinguer les matchs quand plusieurs ordinateurs sont connectés. Le nom de l'ordinateur est utilisé s'il est vide.
3. **Token** (facultatif) : jeton donné par le serveur, envoyé dans l'en-tête `Authorization` de la connexion.
4. Cliquer sur **Connect**. L'état passe à **Connected** (vert).
5. Cocher **Connect when the plugin is loaded** pour se connecter automatiquement à chaque lancement.

Le plugin se reconnecte automatiquement quand la connexion est perdue, et renvoie l'état actuel du match à chaque connexion.

## Regarder un match

Le plugin fonctionne en **partie privée** et en partie hors ligne (match d'exhibition avec des bots). Avec les réglages par défaut, en rejoignant une partie privée :

- vous rejoignez les spectateurs,
- le HUD du jeu est masqué (les noms des joueurs et les infos du match sont gardés), ainsi que les graphes de statistiques (++f10++),
- à la fin du match, vous êtes marqué prêt et le replay est sauvegardé.

Chacune de ces actions peut être désactivée dans les [réglages](configuration.md).

## Essayer sans serveur {#try-it-without-a-server}

Le dépôt contient un petit serveur de test qui affiche tout ce que le plugin envoie, et envoie les commandes tapées dans le terminal :

```bash
pip install websockets
python tools/test_server.py
```

Connecter le plugin à `ws://localhost:3000`, rejoindre un match, et taper `help` dans le terminal pour voir les commandes (`ping`, `focus <joueur>`, `replay <bleu> <orange> <partie>`...).

Ou, sans rien installer, utilisez le [moniteur](monitor.md) dans le navigateur.

!!! tip "Mode debug"
    Activer *Advanced WebSocket settings* → *Debug mode* (`rltm_debug 1`) pour voir chaque message envoyé au serveur et reçu de lui dans la console BakkesMod (++f6++).

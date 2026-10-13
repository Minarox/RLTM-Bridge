# Commandes

Le serveur peut contrôler le jeu en envoyant un message texte JSON :

```json
{ "command": "<nom>", "payload": { ... }, "id": "<facultatif>" }
```

| Champ | Type | Description |
|---|---|---|
| `command` | string | Nom de la commande |
| `payload` | object | Paramètres de la commande, peut être omis quand il n'y en a pas |
| `id` | string \| number | Facultatif, renvoyé dans la [`response`](messages.md#response) pour l'associer à la commande |

Chaque commande reçoit une réponse sur le topic [`response`](messages.md#response) :

```json
{ "topic": "response", "payload": { "command": "ping", "id": "1", "success": true } }
{ "topic": "response", "payload": { "command": "focus_player", "id": "2", "success": false, "error": "Player not found or camera not available" } }
```

## Sécurité {#safety}

Une mauvaise commande ne peut pas faire planter le plugin ni le jeu :

- les messages de plus de **16 Ko** ou de plus de **32 niveaux** d'imbrication sont refusés avant d'être analysés, et le JSON est analysé sans exceptions,
- seules les commandes ci-dessous peuvent être exécutées, **jamais des commandes de la console**,
- les commandes sont mises en file d'attente (**64** au maximum) et exécutées sur le thread du jeu (**8** par image au maximum), jamais depuis le thread réseau,
- chaque paramètre est vérifié, et chaque erreur est renvoyée dans la réponse.

L'utilisateur du plugin peut désactiver toutes les commandes, ou seulement certains groupes, dans les [réglages](../configuration.md#server-control). Une commande désactivée reçoit une erreur en réponse : affichez-la, pour que l'opérateur sache quoi activer.

## Erreurs {#errors}

| `error` | Cause |
|---|---|
| `Message too large` / `JSON too deep` / `Invalid JSON object` | Le message n'est pas une commande valide |
| `"command" must be a string` | `command` est absent ou n'est pas une chaîne |
| `Unknown command "<nom>"` | La commande n'existe pas |
| `Server control is disabled` | Toutes les commandes sont désactivées dans le plugin |
| `Command "<nom>" is disabled in the plugin settings` | Le groupe de la commande est désactivé dans le plugin |
| `Too many pending commands` | Plus de 64 commandes attendent d'être exécutées |
| `"<paramètre>" is required` / `must be a string` / `must be an integer` / `must be between <min> and <max>` | Paramètre invalide |
| Autres messages | La commande n'a pas pu être exécutée, par exemple `Not in a match` |

## Commandes {#commands}

| Commande | Groupe | Description |
|---|---|---|
| [`ping`](#ping) | Toujours autorisée | Vérifier que le plugin répond |
| [`resync`](#resync) | Toujours autorisée | Renvoyer tous les états |
| [`spectate`](#spectate-ready-hide_hud) | Actions du joueur | Rejoindre les spectateurs |
| [`ready`](#spectate-ready-hide_hud) | Actions du joueur | Se marquer prêt |
| [`hide_hud`](#spectate-ready-hide_hud) | Actions du joueur | Masquer le HUD spectateur |
| [`focus_player`](#focus_player) | Caméra spectateur | Centrer la caméra sur un joueur |
| [`set_replay_name`](#set_replay_name) | Noms des replays | Nommer le replay du match |
| [`clear_replay_name`](#set_replay_name) | Noms des replays | Garder le nom par défaut du replay |
| [`set_setting`](#set_setting) | Réglages du jeu | Changer un réglage du plugin |

### `ping`

Vérifier que le plugin répond, par exemple pour afficher l'état de la connexion du jeu dans le serveur. Aucun paramètre.

### `resync`

Renvoyer tous les états (`match`, `scoreboard`, `stats`, `spectator`, `players`, `entities`, `result`), par exemple après le redémarrage d'une partie du serveur. Aucun paramètre.

### `spectate`, `ready`, `hide_hud`

Rejoindre les spectateurs, se marquer prêt, ou masquer le HUD spectateur (avec les éléments gardés dans les réglages), même quand l'action automatique est désactivée dans les réglages. Aucun paramètre. Refusées avec `Not in a match` (ou `Not spectating a match` pour `hide_hud`) quand l'action n'est pas possible.

### `focus_player`

Centrer la caméra spectateur sur un joueur.

| Paramètre | Type | Description |
|---|---|---|
| `uid` | string | `uid` du joueur |
| `name` | string | Nom du joueur |

Au moins l'un des deux est obligatoire ; donnez les deux pour choisir un bot (les bots ont tous le même `uid`).

```json
{ "command": "focus_player", "payload": { "name": "Player1" }, "id": "camera-1" }
```

### `set_replay_name`

Nommer le replay du match en cours, sauvegardé à la fin du match. Envoyé après la fin d'un match, le nom est utilisé pour le match **suivant**. Le nom ne vaut que pour **un** match : envoyez-le pour chaque partie d'une série.

=== "Avec les équipes"

    ```json
    { "command": "set_replay_name", "payload": { "blue": "Équipe 1", "orange": "Équipe 2", "game": 3 } }
    ```

    | Paramètre | Type | Description |
    |---|---|---|
    | `blue` | string | Nom de l'équipe bleue (équipe `0`) |
    | `orange` | string | Nom de l'équipe orange (équipe `1`) |
    | `game` | integer | Numéro de la partie dans la série, de 1 à 99 |

    Le nom est construit avec le réglage `rltm_replay_name_format` du plugin, `{blue} vs {orange} - Match {game}` par défaut : `Équipe 1 vs Équipe 2 - Match 3`.

=== "Avec un nom complet"

    ```json
    { "command": "set_replay_name", "payload": { "name": "Grande finale - Partie 3" } }
    ```

    | Paramètre | Type | Description |
    |---|---|---|
    | `name` | string | Nom du replay |

Le nom est nettoyé (caractères de contrôle et espaces autour retirés) et limité à 128 octets. La réponse contient le nom final :

```json
{ "topic": "response", "payload": { "command": "set_replay_name", "id": null, "success": true, "result": { "name": "Équipe 1 vs Équipe 2 - Match 3", "applied": true } } }
```

`applied` vaut `false` quand il n'y a pas de match en cours ou que le match est terminé : le nom sera donné au replay du match suivant.

`clear_replay_name` oublie le nom : le replay garde le nom par défaut du jeu. Aucun paramètre.

!!! note
    Le replay n'est sauvegardé que si *Save the replay* (`rltm_auto_save_replays`) est activé dans le plugin.

### `set_setting`

Changer un réglage du plugin.

| Paramètre | Type | Description |
|---|---|---|
| `name` | string | Nom de la cvar |
| `value` | string \| number \| boolean | Nouvelle valeur (`true` / `false` pour les cases à cocher) |

Seuls les réglages des groupes *Données*, *Spectateur* et *Fin du match* peuvent être changés : `rltm_send_entities`, `rltm_entities_interval`, `rltm_send_stats`, `rltm_send_spectator`, `rltm_auto_spectate`, `rltm_hide_hud`, `rltm_hud_names`, `rltm_hud_match_info`, `rltm_hide_stat_graphs`, `rltm_auto_ready`, `rltm_auto_save_replays`, `rltm_replay_name_format`. Les réglages de connexion et de contrôle par le serveur ne peuvent pas être changés par le serveur.

```json
{ "command": "set_setting", "payload": { "name": "rltm_entities_interval", "value": 50 } }
```

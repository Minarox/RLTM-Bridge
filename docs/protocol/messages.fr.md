# Messages

Chaque message envoyé par le plugin : `{ "topic": "<topic>", "payload": <données> }`. Voir la [vue d'ensemble](index.md) pour les topics d'état et ponctuels, l'identité des joueurs et les unités.

| Topic | Sorte | Envoyé |
|---|---|---|
| [`plugin`](#plugin) | Ponctuel | En premier, à chaque connexion |
| [`match`](#match) | État | À chaque changement du match (chaque seconde tant que le temps s'écoule) |
| [`scoreboard`](#scoreboard) | État | Quand les statistiques d'un joueur changent |
| [`stats`](#stats) | État | Chaque seconde pendant le jeu |
| [`spectator`](#spectator) | État | Quand la cible de la caméra change |
| [`players`](#players) | État | Quand un joueur arrive, part, ou change d'équipe ou de voiture |
| [`entities`](#entities) | État | Toutes les 100 ms (par défaut) pendant le jeu |
| [`statistic`](#statistic) | Ponctuel | À chaque événement de stat d'un joueur (but, arrêt, démolition...) |
| [`event`](#event) | Ponctuel | À chaque événement du match (coup d'envoi, but, prolongation...) |
| [`result`](#result) | État | Une fois, à la fin du match |
| [`response`](#response) | Ponctuel | Réponse à une [commande](commands.md) |

Quand il n'y a plus de match, `match`, `scoreboard`, `stats`, `spectator`, `players` et `entities` sont envoyés avec un payload `null`.

---

## `plugin`

Envoyé en premier à chaque connexion.

| Champ | Type | Description |
|---|---|---|
| `version` | string | Version du plugin |
| `protocol` | number | Version du protocole, actuellement `2` |
| `client` | string | Nom du client (le nom du client des réglages, ou le nom de l'ordinateur) |

```json
{ "topic": "plugin", "payload": { "version": "1.1.0.42", "protocol": 2, "client": "CASTER-1" } }
```

---

## `match`

État du match.

| Champ | Type | Description |
|---|---|---|
| `guid` | string \| null | Identifiant unique du match donné par le jeu, `null` s'il n'en donne pas (matchs hors ligne) |
| `arenaCode` | string | Code de l'arène, par exemple `Stadium_P` |
| `score` | number[] | `[bleu, orange]` |
| `teams` | object[] | Équipes, voir ci-dessous |
| `clock` | number | Secondes restantes, figées quand le match se termine en prolongation |
| `duration` | number | Durée du match dans les réglages de la partie (secondes) |
| `isUnlimited` | boolean | Temps illimité |
| `isStarted` | boolean | `true` à partir du premier coup d'envoi |
| `isPaused` | boolean | Le match est en pause |
| `isOvertime` | boolean | Prolongation |
| `isEnded` | boolean | Le match est terminé |
| `isReplay` | boolean | Un replay de but est en cours |

Une équipe :

| Champ | Type | Description |
|---|---|---|
| `index` | number | `0` bleu, `1` orange |
| `name` | string \| null | Nom personnalisé de l'équipe réglé dans la partie privée, `null` s'il n'est pas réglé |
| `primaryColor` | string | Couleur principale, `#RRGGBB` |
| `secondaryColor` | string | Couleur secondaire, `#RRGGBB` |

```json
{
  "topic": "match",
  "payload": {
    "guid": "D3A6F8C24B1E4E0F9A2B6C7D8E9F0A1B",
    "arenaCode": "Stadium_P",
    "score": [1, 2],
    "teams": [
      { "index": 0, "name": null, "primaryColor": "#1873FF", "secondaryColor": "#0E4AA8" },
      { "index": 1, "name": null, "primaryColor": "#FF8A15", "secondaryColor": "#B35A00" }
    ],
    "clock": 183,
    "duration": 300,
    "isUnlimited": false,
    "isStarted": true,
    "isPaused": false,
    "isOvertime": false,
    "isEnded": false,
    "isReplay": false
  }
}
```

---

## `scoreboard`

Statistiques de chaque joueur, indexées par la [clé du joueur](index.md#players). Objet vide tant que le match n'a pas commencé.

| Champ | Type | Description |
|---|---|---|
| `uid`, `name`, `bot`, `teamIndex` | | [Identité du joueur](index.md#players) |
| `mvp` | boolean | MVP du match |
| `score` | number | Score du tableau des scores |
| `goals`, `shots`, `assists`, `saves` | number | Compteurs du tableau des scores |
| `ballTouches`, `carTouches` | number | Touches de balle et des autres voitures |
| `events` | object | Nombre de chaque événement de stat reçu pour le joueur depuis le début du match (`Goal`, `Shot`, `Save`, `EpicSave`, `Demolish`, `AerialGoal`...). **Un événement absent vaut 0.** |

```json
{
  "topic": "scoreboard",
  "payload": {
    "Steam|76561198000000000|0|Player1": {
      "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0,
      "mvp": false, "score": 320, "goals": 1, "shots": 2, "assists": 0, "saves": 1,
      "ballTouches": 24, "carTouches": 3,
      "events": { "Shot": 2, "Goal": 1, "Save": 1, "Demolish": 2 }
    }
  }
}
```

---

## `stats`

Statistiques avancées calculées par le plugin à chaque image, pendant que la balle est en jeu (du premier contact après un coup d'envoi jusqu'au but suivant, sans les replays de but). Indexées par la [clé du joueur](index.md#players), envoyées chaque seconde pendant le jeu (seulement quand une valeur change). Désactivables avec `rltm_send_stats`, toujours incluses dans le [`result`](#result).

| Champ | Type | Description |
|---|---|---|
| `uid`, `name`, `bot`, `teamIndex` | | [Identité du joueur](index.md#players) |
| `boostPickups` | number | Pastilles de boost ramassées |
| `boostUsed` | number | Boost utilisé, `100` par plein |
| `timeInPlay` | number | Secondes avec une voiture sur le terrain pendant que la balle est en jeu |
| `timeSupersonic` | number | Secondes en vitesse supersonique |
| `timeOnGround`, `timeOnWall`, `timeInAir` | number | Secondes au sol, sur un mur, en l'air |
| `averageSpeed` | number | Vitesse moyenne (km/h) |

```json
{
  "topic": "stats",
  "payload": {
    "Steam|76561198000000000|0|Player1": {
      "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0,
      "boostPickups": 12, "boostUsed": 410, "timeInPlay": 245.3, "timeSupersonic": 31.2,
      "timeOnGround": 180.5, "timeOnWall": 12.1, "timeInAir": 52.7, "averageSpeed": 64
    }
  }
}
```

---

## `spectator`

Joueur suivi par la caméra spectateur. Désactivable avec `rltm_send_spectator`.

| Champ | Type | Description |
|---|---|---|
| `target` | object \| null | [Identité du joueur](index.md#players) suivi par la caméra, **`null` en caméra libre** (ou en caméra balle) |
| `camera` | string | Nom brut de l'état de la caméra donné par le jeu |

```json
{ "topic": "spectator", "payload": { "camera": "…", "target": { "uid": "Steam|…", "name": "Player1", "bot": false, "teamIndex": 0 } } }
```

!!! tip "Overlay du joueur regardé"
    Affichez les statistiques de `target` (depuis le [`scoreboard`](#scoreboard) et les [`stats`](#stats), avec la clé du joueur), et cachez l'overlay quand `target` vaut `null`. Attendez ~150 ms avant de le cacher, pour éviter un clignotement si la caméra passe par `null` en changeant de joueur.

---

## `players`

Tous les joueurs du match, spectateurs compris (`teamIndex` 255).

| Champ | Type | Description |
|---|---|---|
| `uid`, `name`, `bot`, `teamIndex` | | [Identité du joueur](index.md#players) |
| `carId` | number | Identifiant du modèle de la voiture, `0` sans voiture |

```json
{ "topic": "players", "payload": [
  { "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0, "carId": 23 }
] }
```

---

## `entities`

Balles et voitures, envoyées toutes les `rltm_entities_interval` ms (100 par défaut) pendant le jeu : match commencé, ni en pause, ni terminé. Désactivables avec `rltm_send_entities`.

| Champ | Type | Description |
|---|---|---|
| `time` | number | Temps monotone de l'échantillon (ms) : utilisez la différence entre deux envois pour interpoler les positions |
| `balls` | object[] | `speed` (km/h) et `location` (`[X, Y, Z]`) de chaque balle |
| `cars` | object[] | Voitures, voir ci-dessous |

Une voiture :

| Champ | Type | Description |
|---|---|---|
| `uid`, `name`, `bot`, `teamIndex` | | [Identité du joueur](index.md#players) |
| `speed` | number | km/h |
| `location` | number[] | `[X, Y, Z]` |
| `boost` | number | `0` à `100` |
| `isSuperSonic`, `isOnGround`, `isOnWall`, `isInGoal`, `isDodging`, `hasFlip` | boolean | État de la voiture |

```json
{
  "topic": "entities",
  "payload": {
    "time": 123456789,
    "balls": [ { "speed": 87, "location": [120, -2300, 93] } ],
    "cars": [ {
      "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0,
      "speed": 154, "location": [-1500, 800, 17], "boost": 33,
      "isSuperSonic": false, "isOnGround": true, "isOnWall": false,
      "isInGoal": false, "isDodging": false, "hasFlip": true
    } ]
  }
}
```

!!! note
    Quand le serveur ne lit pas les messages assez vite, le plugin saute des envois d'entités (de toute façon dépassés à l'envoi suivant) au lieu de ralentir le jeu : ne supposez pas un intervalle fixe, utilisez `time`.

---

## `statistic`

Événement de stat d'un joueur, tel qu'affiché par le jeu. Suivi d'un [`scoreboard`](#scoreboard) à jour une fois le match commencé.

| Champ | Type | Description |
|---|---|---|
| `uid`, `name`, `bot` | | [Identité du joueur](index.md#players) |
| `eventName` | string | Nom de l'événement : `Goal`, `Assist`, `Save`, `EpicSave`, `Shot`, `Demolish`, `Demolition`, `Center`, `Clear`, `AerialGoal`, `OwnGoal`... |

```json
{ "topic": "statistic", "payload": { "uid": "Steam|…", "name": "Player1", "bot": false, "eventName": "Demolish" } }
```

---

## `event`

Événements du match, pour déclencher les animations des overlays. L'état reste dans les autres topics.

Chaque événement a un `type` et le `clock` (secondes restantes) du match :

| `type` | Données | Envoyé |
|---|---|---|
| `kickoff` | | Début de chaque compte à rebours du coup d'envoi |
| `goal` | `teamIndex`, `scorer`, `assister`, `isOwnGoal`, `ballSpeed` | ~0,5 s après chaque but, voir ci-dessous |
| `overtime` | | Début de la prolongation |
| `replayStart` / `replayEnd` | | Début et fin d'un replay de but |
| `matchEnded` | `winner` (`0`, `1` ou `null`), `score` | Fin du match, suivie du [`result`](#result) |
| `podium` | | Début de l'écran du podium |

Un but :

| Champ | Type | Description |
|---|---|---|
| `teamIndex` | number | Équipe qui marque |
| `scorer` | object \| null | [Identité du joueur](index.md#players) qui marque, `null` si l'information n'a pas été reçue |
| `assister` | object \| null | Joueur de la passe décisive, `null` sans passe |
| `isOwnGoal` | boolean | But contre son camp : `scorer` est le joueur qui a marqué contre son équipe |
| `ballSpeed` | number | Vitesse de la balle (km/h) |

```json
{ "topic": "event", "payload": {
  "type": "goal", "clock": 120, "teamIndex": 0, "isOwnGoal": false, "ballSpeed": 112,
  "scorer": { "uid": "Steam|…", "name": "Player1", "bot": false, "teamIndex": 0 },
  "assister": null
} }
```

---

## `result`

Résultat complet du match, envoyé **une seule fois** à la fin du match, après son dernier but. Utilisez-le pour enregistrer les résultats d'un tournoi.

| Champ | Type | Description |
|---|---|---|
| `guid` | string \| null | Identifiant unique du match (voir [`match`](#match)) |
| `client` | string | Nom du client |
| `endedAt` | number | Fin du match (ms depuis 1970) |
| `arenaCode` | string | Code de l'arène |
| `duration` | number | Durée du match dans les réglages de la partie (secondes) |
| `timePlayed` | number | Temps réellement joué, prolongation comprise (secondes) |
| `isOvertime` | boolean | Le match est allé en prolongation |
| `score` | number[] | `[bleu, orange]` |
| `winner` | number \| null | Équipe gagnante, `null` si le jeu ne l'a pas déterminée |
| `mvp` | object \| null | [Identité du joueur](index.md#players) MVP |
| `teams` | object[] | Équipes (voir [`match`](#match)) |
| `scoreboard` | object | [`scoreboard`](#scoreboard) final |
| `stats` | object | [`stats`](#stats) avancées finales |
| `replayName` | string \| null | Nom donné au replay avec [`set_replay_name`](commands.md#set_replay_name) |

!!! important "Dédupliquer les résultats"
    Le `result` est gardé par le plugin et **renvoyé à chaque connexion jusqu'à la fin du match suivant**, pour qu'un serveur redémarré après un match le reçoive quand même. Dédupliquez-le avec son `guid` (ou `client` + `endedAt` s'il n'a pas de `guid`).

---

## `response`

Réponse à une [commande](commands.md) du serveur.

| Champ | Type | Description |
|---|---|---|
| `command` | string | Nom de la commande, vide si le message était invalide |
| `id` | string \| number \| null | `id` de la commande |
| `success` | boolean | La commande a été exécutée |
| `error` | string | Seulement si `success` vaut `false` : pourquoi la commande a été refusée |
| `result` | object | Seulement pour certaines commandes, voir [Commandes](commands.md) |

```json
{ "topic": "response", "payload": { "command": "focus_player", "id": "42", "success": false, "error": "Player not found or camera not available" } }
```

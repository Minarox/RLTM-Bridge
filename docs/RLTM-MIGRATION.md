# Instructions : mettre à jour RLTM pour le protocole v2 du plugin WSC

Ce fichier est destiné à une session Claude qui travaille sur **RLTM**, le serveur WebSocket qui reçoit les données du plugin BakkesMod **WSC** (dépôt `Minarox/WSC-Plugin`, branche `dev`) pour les overlays OBS et la gestion de tournois.

Le format des messages du plugin a changé. Ta mission : adapter RLTM à ce nouveau format (**protocole v2**), sans casser les overlays existants.

## Avant de commencer

1. Explore RLTM et repère tout le code qui reçoit ou utilise les messages du plugin. Cherche les topics `match`, `players`, `entities`, `statistic`, les champs `statistics`, `asFlip`, et les clés de joueur `"<uid>|<name>"`.
2. Fais la liste des fichiers concernés (serveur, stores, overlays, tests) avant de modifier quoi que ce soit.
3. Si le serveur utilise la librairie Node `ws`, vérifie l'option `perMessageDeflate`. Elle est désactivée par défaut côté serveur alors que le plugin propose la compression. L'activer divise par ~10 la taille des messages sur le réseau.
4. En cas de doute sur un comportement du plugin, la référence est le code de `Minarox/WSC-Plugin` (branche `dev`) : `WSC/Game/GameData.cpp` (données envoyées), `WSC/WebSocket/WebSocket.cpp` (envoi) et `WSC/WebSocket/ServerCommands.cpp` (commandes).

## Ce qui change (v1 → v2)

| # | Changement | Impact RLTM |
|---|---|---|
| 1 | Les statistiques des joueurs quittent `match.statistics` pour un nouveau topic **`scoreboard`** | **Incompatible** : lire les stats depuis `scoreboard` |
| 2 | Les 40 compteurs d'événements (`Demolish`, `EpicSave`…) ne sont plus à plat dans chaque joueur, mais dans un objet **`events`** qui ne contient **que les événements reçus** | **Incompatible** : `player.events.Demolish ?? 0` au lieu de `player.Demolish` |
| 3 | `entities.cars[].asFlip` est renommé **`hasFlip`** | **Incompatible** |
| 4 | Nouveau champ **`entities.time`** (ms, horloge monotone) | Nouveau : permet d'interpoler les positions |
| 5 | Nouveau topic **`plugin`**, envoyé en premier à chaque connexion : `{ version, protocol }` | Nouveau : vérifier `protocol === 2` |
| 6 | Le plugin accepte des **commandes** du serveur et répond sur le topic **`response`** | Nouveau : optionnel |
| 7 | Nouveau topic **`spectator`** : le joueur suivi par la caméra spectateur, `null` en caméra libre | Nouveau : overlay dynamique du joueur regardé |

Un plugin v1 n'envoie jamais de message `plugin`. Si des messages arrivent sans `plugin` préalable, c'est un ancien plugin : affiche un avertissement clair (« plugin WSC trop ancien, mettez-le à jour »). Ne cherche pas à gérer les deux formats.

## Format complet des messages (plugin → serveur)

Chaque message est un objet JSON `{ "topic": "<topic>", "payload": <données> }`.

Les topics **d'état** (`match`, `scoreboard`, `spectator`, `players`, `entities`) ne sont envoyés que lorsqu'ils changent, et sont tous renvoyés à chaque (re)connexion. Un payload `null` signifie « plus de match » : il faut vider l'état correspondant. Les topics `plugin`, `statistic` et `response` sont des événements ponctuels.

### `plugin` (à la connexion)
```json
{ "topic": "plugin", "payload": { "version": "1.0.0.642", "protocol": 2 } }
```

### `match`
```json
{
  "topic": "match",
  "payload": {
    "arenaCode": "Stadium_P",
    "score": [1, 2],
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
`score` vaut `[équipe 0, équipe 1]`. `clock` est le nombre de secondes restantes, figé quand le match se termine en prolongation. Le champ `statistics` **n'existe plus**.

### `scoreboard` (nouveau)
Objet indexé par la clé de joueur `"<uid>|<name>"` (les bots partagent le même uid, d'où le nom). Il vaut `{}` tant que le match n'a pas commencé.
```json
{
  "topic": "scoreboard",
  "payload": {
    "Steam|76561198000000000|0|Player1": {
      "uid": "Steam|76561198000000000|0",
      "name": "Player1",
      "bot": false,
      "teamIndex": 0,
      "mvp": false,
      "score": 320,
      "goals": 1,
      "shots": 2,
      "assists": 0,
      "saves": 1,
      "ballTouches": 24,
      "carTouches": 3,
      "events": { "Shot": 2, "Goal": 1, "Save": 1, "Demolish": 2 }
    }
  }
}
```
`events` contient **tous** les événements du stat ticker reçus pour le joueur depuis le début du match, y compris des noms qui n'étaient pas dans l'ancienne liste fixe (`Goal`, `Shot`, `Save`, `Assist`…). Un événement absent vaut 0.

### `spectator` (nouveau)
Le joueur que suit la caméra spectateur. Le message n'est envoyé que lorsque la cible ou le mode de caméra change.
```json
{ "topic": "spectator", "payload": {
  "camera": "<mode de caméra du jeu>",
  "target": { "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0 }
} }
```
- `target` vaut **`null`** quand la caméra ne suit aucun joueur (caméra libre, caméra sur la balle…). C'est le signal pour **cacher** l'overlay du joueur regardé.
- Le payload entier vaut `null` quand il n'y a pas de match.
- `camera` est le nom brut du mode de caméra renvoyé par le jeu. Ses valeurs exactes n'ont pas encore été relevées en jeu : **ne base pas l'affichage dessus**, utilise `target`. Tu peux le logger pour aider à documenter ses valeurs.
- Pour afficher les stats du joueur regardé, croise `target` avec le `scoreboard` via la clé `"<uid>|<name>"`.
- Ajoute un léger délai (~150 ms) avant de cacher l'overlay, pour éviter un clignotement si le jeu passe brièvement par `null` en changeant de joueur.

### `players`
```json
{ "topic": "players", "payload": [
  { "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0, "carId": 23 }
] }
```
Contient aussi les spectateurs (`teamIndex` = 255).

### `entities`
Envoyé toutes les 100 ms par défaut (réglable côté plugin), seulement pendant le jeu : match commencé, ni en pause, ni terminé.
```json
{
  "topic": "entities",
  "payload": {
    "time": 123456789,
    "balls": [ { "speed": 87, "location": [120, -2300, 93] } ],
    "cars": [ {
      "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0,
      "speed": 154, "location": [-1500, 800, 17], "boost": 33,
      "isSuperSonic": false, "isOnWall": false, "isOnGround": true,
      "isInGoal": false, "isDodging": false, "hasFlip": true
    } ]
  }
}
```
- `speed` est en km/h, `location` en unités Unreal `[X, Y, Z]`, et `boost` va de 0 à 100.
- `time` est une horloge monotone en millisecondes : **seule la différence entre deux messages a un sens**. Ne l'affiche pas comme une heure. Pour une minimap fluide, interpole les positions entre les deux derniers échantillons selon le temps écoulé.
- Le plugin peut sauter des mises à jour si le serveur ne lit pas assez vite. L'intervalle entre deux messages n'est donc pas garanti : utilise `time`, pas un intervalle supposé.

### `statistic` (événement ponctuel)
```json
{ "topic": "statistic", "payload": { "uid": "Steam|...", "name": "Player1", "bot": false, "eventName": "Demolish" } }
```
Si le match a commencé, il est suivi d'un `scoreboard` à jour. Utilise `statistic` pour les animations (« Player1 a démoli X ») et `scoreboard` pour les compteurs.

### `response`
Réponse à une commande envoyée par le serveur (voir ci-dessous).
```json
{ "topic": "response", "payload": { "command": "focus_player", "id": "42", "success": false, "error": "Player not found or camera not available" } }
```
`error` n'est présent que si `success` vaut `false`. `command` et `id` sont vides ou `null` quand le message reçu était invalide.

## Commandes (serveur → plugin), optionnel

Format : `{ "command": "<nom>", "payload": { ... }, "id": "<optionnel, chaîne ou nombre>" }`. Le plugin répond avec le même `id`.

| Commande | Payload | Effet |
|---|---|---|
| `ping` | | Vérifier que le plugin répond |
| `resync` | | Renvoyer `match`, `scoreboard`, `spectator`, `players` et `entities` |
| `spectate` | | Passer en spectateur |
| `ready` | | Se marquer prêt |
| `hide_hud` | | Masquer le HUD spectateur |
| `focus_player` | `{ "uid": "...", "name": "..." }` (au moins un des deux) | Centrer la caméra spectateur sur un joueur |
| `set_setting` | `{ "name": "<cvar>", "value": <valeur> }` | Seuls `wsc_entities_interval`, `wsc_auto_save_replays`, `wsc_auto_spectate`, `wsc_auto_ready`, `wsc_hide_hud` et `wsc_hide_stat_graphs` sont acceptés |

Limites côté plugin : messages de 16 Ko maximum, 32 niveaux d'imbrication maximum, 64 commandes en attente maximum, et toute commande inconnue est refusée. Le joueur peut désactiver toutes les commandes (`wsc_server_control`). Gère donc toujours `success: false`.

Une idée de fonctionnalité si RLTM a une interface de régie : boutons « caméra sur ce joueur » (`focus_player`) et indicateur de connexion (`ping` régulier avec un `id`).

## Travail attendu

1. Adapter la réception des messages : nouveau topic `scoreboard`, objet `events`, renommage `hasFlip`, vérification du topic `plugin`.
2. Vider l'état local quand un payload d'état vaut `null` (fin de match).
3. Adapter les overlays et calculs de stats (tournois) qui lisaient `match.statistics` ou les compteurs à plat. Un compteur absent vaut 0.
4. Créer (ou adapter) un overlay « joueur regardé » piloté par le topic `spectator` : afficher les stats du `target` (depuis `scoreboard`), et le cacher quand `target` vaut `null`.
5. Utiliser `entities.time` pour l'interpolation là où des positions sont affichées (minimap, etc.).
6. Activer `perMessageDeflate` sur le serveur WebSocket si ce n'est pas le cas.
7. Optionnel : envoyer des commandes et gérer les réponses.
8. Mettre à jour les types, les tests et la documentation de RLTM. Si RLTM a des tests, ajoute des fixtures construites à partir des exemples ci-dessus.

À la fin, résume les fichiers modifiés, ce qui a été testé, et tout point du protocole qui t'a semblé ambigu ou manquant : il pourra être corrigé côté plugin.

# Instructions : mettre à jour RLTM pour le protocole v2 du plugin RLTM Bridge

Ce fichier est destiné à une session Claude qui travaille sur **RLTM**, le serveur WebSocket qui reçoit les données du plugin BakkesMod **RLTM Bridge**, anciennement WSC (dépôt `Minarox/RLTM-Bridge`, branche `dev`), pour les overlays OBS et la gestion de tournois.

Le format des messages du plugin a changé. Ta mission : adapter RLTM à ce nouveau format (**protocole v2**), sans casser les overlays existants.

## Avant de commencer

1. Explore RLTM et repère tout le code qui reçoit ou utilise les messages du plugin. Cherche les topics `match`, `players`, `entities`, `statistic`, les champs `statistics`, `asFlip`, et les clés de joueur `"<uid>|<name>"`.
2. Fais la liste des fichiers concernés (serveur, stores, overlays, tests) avant de modifier quoi que ce soit.
3. La compression (`perMessageDeflate`) est déjà activée dans RLTM : garde-la, elle divise par ~10 la taille des messages sur le réseau.
4. En cas de doute sur un comportement du plugin, la référence est le code de `Minarox/RLTM-Bridge` (branche `dev`) : `RLTMBridge/Game/GameData.cpp` (données envoyées), `RLTMBridge/WebSocket/WebSocket.cpp` (envoi) et `RLTMBridge/WebSocket/ServerCommands.cpp` (commandes).

## État actuel de RLTM (branche `develop`)

Travaille sur la branche **`develop`** de RLTM (`main` est en retard). Ce qui y concerne le plugin, relevé au moment de la rédaction :

- **`src/controllers/plugin.ts`** reçoit le plugin sur `/` avec le jeton du tournoi dans l'URL (`?token=…`, vérifié dans `beforeHandle`). C'est l'URL à saisir dans le plugin : `ws://<hôte>:<port>/?token=<jeton>`. Le jeton dans l'URL reste valable ; l'en-tête `Authorization: Bearer` du plugin (`rltm_token`) est une alternative facultative.
- **Un seul plugin par tournoi** : une nouvelle connexion remplace la précédente (`app.store[tournamentId][Topic.PLUGIN]`), et les messages de l'ancienne reçoivent `{ "valid": false }`. Pour plusieurs matchs en parallèle, utilise le nom du client (`plugin.client` / en-tête `X-RLTM-Client`) pour associer chaque connexion à un match.
- **RLTM envoie `{ "valid": true }` à l'ouverture** (et `{ "valid": false }` au plugin remplacé). Ce ne sont pas des commandes : le plugin y répond par un `response` d'erreur (`"command" must be a string`). C'est sans danger, mais supprime ces messages (ou remplace-les par un `ping` avec un `id` si tu veux vérifier que le plugin répond).
- **`src/types/plugin.ts`** décrit encore le protocole v1 : `MatchPayload.statistics`, les 40 compteurs à plat dans `StatisticsPayload`, `Cars.asFlip`, `Balls.radius` (que le plugin n'envoie plus). Le contrôleur enregistre le match et les statistiques en base à chaque message `match` (`createOrUpdateMatch`, `createOrUpdateStatistic`) : ce traitement doit passer sur `scoreboard`, et l'enregistrement final idéalement sur `result`.
- **`Topic.PLUGIN = "plugin"`** (`src/types/app.ts`) sert déjà, côté RLTM, à publier aux overlays l'identifiant du plugin connecté. Le plugin envoie maintenant lui aussi un topic `plugin` (version, protocole, client) : ne mélange pas les deux.
- `perMessageDeflate` est déjà activé (`src/index.ts`), avec `idleTimeout: 3` : le plugin envoie un ping chaque seconde par défaut (`rltm_ping_interval`), garde-le en dessous de 3 s.

La documentation complète du protocole est publiée sur le site du plugin (sources dans `docs/protocol/` du dépôt `Minarox/RLTM-Bridge`) : elle fait référence en cas de différence avec ce fichier.

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
| 8 | Nouvelle commande **`set_replay_name`** : nommer le replay du match avec les équipes et le numéro de la partie | Nouveau : à envoyer pour chaque partie d'une série |
| 9 | Nouveaux champs **`match.guid`** (identifiant unique du match) et **`match.teams`** (noms personnalisés et couleurs des équipes) | Nouveau |
| 10 | Nouveau topic **`stats`** : statistiques avancées (boost, supersonique, temps au sol/au mur/en l'air, vitesse moyenne) | Nouveau : stats de tournoi |
| 11 | Nouveau topic **`event`** : `kickoff`, `goal` (buteur, passeur, vitesse), `overtime`, `replayStart`, `replayEnd`, `matchEnded`, `podium` | Nouveau : animations des overlays |
| 12 | Nouveau topic **`result`** : résultat complet de la partie, envoyé une fois à la fin | Nouveau : **à utiliser pour enregistrer les résultats de tournoi** |
| 13 | Le message `plugin` contient le nom du client (`client`), et la connexion porte les en-têtes `X-RLTM-Client` et `Authorization: Bearer <jeton>` | Nouveau : identifier chaque PC, refuser un client inconnu |
| 14 | Le plugin s'appelle désormais **RLTM Bridge** (anciennement WSC) : en-tête `X-RLTM-Client` (et non `X-WSC-Client`), réglages `rltm_*` (et non `wsc_*`), dépôt `Minarox/RLTM-Bridge` | À prendre en compte dans les textes et les noms de réglages envoyés avec `set_setting` |

Un plugin v1 n'envoie jamais de message `plugin`. Si des messages arrivent sans `plugin` préalable, c'est un ancien plugin : affiche un avertissement clair (« plugin RLTM Bridge trop ancien, mettez-le à jour »). Ne cherche pas à gérer les deux formats.

## Format complet des messages (plugin → serveur)

Chaque message est un objet JSON `{ "topic": "<topic>", "payload": <données> }`.

Les topics **d'état** (`match`, `scoreboard`, `stats`, `spectator`, `players`, `entities`, `result`) ne sont envoyés que lorsqu'ils changent, et sont tous renvoyés à chaque (re)connexion. Un payload `null` signifie « plus de match » : il faut vider l'état correspondant. Les topics `plugin`, `statistic`, `event` et `response` sont des événements ponctuels. Exception : `result` n'est pas remis à `null` à la fin du match (voir plus bas).

### `plugin` (à la connexion)
```json
{ "topic": "plugin", "payload": { "version": "1.0.0.642", "protocol": 2, "client": "PC-CASTER-1" } }
```

### Identification des clients
Quand plusieurs matchs se jouent en même temps, chaque PC spectateur a son plugin connecté à RLTM. Pour les distinguer :
- `client` (dans `plugin`, `result`, et l'en-tête HTTP `X-RLTM-Client` de la connexion) est le nom choisi dans le plugin (`rltm_client_name`), ou à défaut le nom de l'ordinateur. **Associe chaque connexion à un client**, puis chaque client à un match du tournoi.
- Si un jeton est configuré dans le plugin (`rltm_token`), la connexion porte l'en-tête `Authorization: Bearer <jeton>`. RLTM peut refuser la connexion (pendant le handshake) si le jeton est inconnu. Le jeton n'est jamais envoyé dans les messages.

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
`score` vaut `[équipe 0, équipe 1]`. Nouveaux champs :
- `guid` : identifiant unique du match donné par le jeu, `null` si le jeu n'en donne pas (matchs hors ligne).
- `teams` : `[{ "index": 0, "name": "Mon équipe" | null, "primaryColor": "#1873FF", "secondaryColor": "#…" }, …]`. `name` vaut `null` si aucun nom personnalisé n'est réglé dans la partie privée : RLTM garde alors ses propres noms. Les couleurs n'ont pas encore été vérifiées en jeu : si elles semblent fausses (trop sombres), signale-le.

 `clock` est le nombre de secondes restantes, figé quand le match se termine en prolongation. Le champ `statistics` **n'existe plus**.

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
- L'envoi des entités peut être désactivé dans le plugin (`rltm_send_entities`) : le topic vaut alors `null`. Il en va de même pour `spectator` (`rltm_send_spectator`). Un overlay qui en dépend doit afficher un état vide, pas planter.
- Le plugin peut sauter des mises à jour si le serveur ne lit pas assez vite. L'intervalle entre deux messages n'est donc pas garanti : utilise `time`, pas un intervalle supposé.

### `stats` (nouveau)
Statistiques avancées calculées par le plugin à chaque image pendant que la balle est en jeu (du premier contact après un coup d'envoi jusqu'au but suivant). Envoyées toutes les secondes pendant le jeu, et toujours incluses dans `result`. Objet indexé par la clé de joueur `"<uid>|<name>"` :
```json
{ "topic": "stats", "payload": { "Steam|…|Player1": {
  "uid": "Steam|…", "name": "Player1", "bot": false, "teamIndex": 0,
  "boostPickups": 12, "boostUsed": 410, "timeInPlay": 245.3, "timeSupersonic": 31.2,
  "timeOnGround": 180.5, "timeOnWall": 12.1, "timeInAir": 52.7, "averageSpeed": 64
} } }
```
`boostUsed` est en unités de jauge (100 = un plein), les temps en secondes, `averageSpeed` en km/h. Peut valoir `null` si l'envoi est désactivé dans le plugin (`rltm_send_stats`).

### `event` (nouveau, événements ponctuels)
`{ "type": "<type>", "clock": <secondes restantes>, … }` :
- `kickoff` : début d'un coup d'envoi (compte à rebours).
- `goal` : `{ "teamIndex": 0, "scorer": { uid, name, bot, teamIndex } | null, "assister": … | null, "isOwnGoal": false, "ballSpeed": 112 }`. Envoyé ~0,5 s après le but, le temps de recevoir le buteur et le passeur. `scorer` ou `assister` peuvent être `null` (pas de passe, ou information non reçue).
- `overtime` : début de la prolongation.
- `replayStart` / `replayEnd` : début et fin du replay du but.
- `matchEnded` : `{ "winner": 0 | 1 | null, "score": [a, b] }`, suivi du `result`.
- `podium` : début de l'écran du podium.

Ce sont des déclencheurs d'animation : l'état reste dans les autres topics (`match.isReplay`, etc.).

### `result` (nouveau)
Résultat complet de la partie, envoyé **une seule fois** à la fin du match (après le dernier but) :
```json
{ "topic": "result", "payload": {
  "guid": "…", "client": "PC-CASTER-1", "endedAt": 1760000000000, "arenaCode": "Stadium_P",
  "duration": 300, "timePlayed": 342, "isOvertime": true, "score": [3, 2], "winner": 0,
  "mvp": { "uid": "…", "name": "Player1", "bot": false, "teamIndex": 0 },
  "teams": [ … ], "scoreboard": { … }, "stats": { … }, "replayName": "Équipe 1 vs Équipe 2 - Match 3"
} }
```
- `endedAt` est l'heure de fin (ms depuis 1970), `timePlayed` le temps de jeu réel en secondes (prolongation comprise).
- `winner` est l'index de l'équipe gagnante, `null` si le jeu ne l'a pas déterminé.
- **Le `result` est gardé et renvoyé à chaque reconnexion jusqu'à la fin du match suivant** (même après la fermeture du match), pour qu'un RLTM redémarré ne perde pas un résultat. **Déduplique-le avec `guid`** (et à défaut `client` + `endedAt`).

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
`error` n'est présent que si `success` vaut `false`. Certaines commandes renvoient aussi un champ `result` (voir `set_replay_name`). `command` et `id` sont vides ou `null` quand le message reçu était invalide.

## Commandes (serveur → plugin), optionnel

Format : `{ "command": "<nom>", "payload": { ... }, "id": "<optionnel, chaîne ou nombre>" }`. Le plugin répond avec le même `id`.

| Commande | Payload | Effet |
|---|---|---|
| `ping` | | Vérifier que le plugin répond |
| `resync` | | Renvoyer tous les états (`match`, `scoreboard`, `stats`, `spectator`, `players`, `entities`, `result`) |
| `spectate` | | Passer en spectateur |
| `ready` | | Se marquer prêt |
| `hide_hud` | | Masquer le HUD spectateur |
| `focus_player` | `{ "uid": "...", "name": "..." }` (au moins un des deux) | Centrer la caméra spectateur sur un joueur |
| `set_setting` | `{ "name": "<cvar>", "value": <valeur> }` | Changer un réglage de données, de spectateur ou de fin de match : `rltm_send_entities`, `rltm_entities_interval`, `rltm_send_spectator`, `rltm_auto_spectate`, `rltm_hide_hud`, `rltm_hud_names`, `rltm_hud_match_info`, `rltm_hide_stat_graphs`, `rltm_auto_ready`, `rltm_auto_save_replays`, `rltm_replay_name_format` |
| `set_replay_name` | `{ "blue": "<équipe>", "orange": "<équipe>", "game": <numéro> }` ou `{ "name": "<nom complet>" }` | Nommer le replay du match (voir ci-dessous) |
| `clear_replay_name` | | Revenir au nom par défaut du jeu |

### Nommer les replays (`set_replay_name`)

Le jeu ne connaît pas les équipes de RLTM. Pour que chaque replay sauvegardé porte un nom du style « Équipe 1 vs Équipe 2 - Match 3 », RLTM doit envoyer **pour chaque partie** d'une série :
```json
{ "command": "set_replay_name", "payload": { "blue": "Équipe 1", "orange": "Équipe 2", "game": 3 }, "id": "replay-3" }
```
- `blue` est l'équipe 0 (bleue) du jeu, `orange` l'équipe 1. RLTM doit donc savoir quelle équipe joue de quel côté : utilise le `teamIndex` des joueurs (`players` / `scoreboard`) pour faire la correspondance avec ses équipes.
- `game` est le numéro de la partie dans la série : un entier de 1 à 99.
- Le plugin construit le nom avec sa cvar `rltm_replay_name_format` (par défaut `{blue} vs {orange} - Match {game}`), réglable dans le jeu. Pour imposer un nom complet, envoie `{ "name": "..." }` à la place.
- Le nom est nettoyé (caractères de contrôle retirés) et limité à 128 octets.
- **Quand l'envoyer** : dès que les équipes de la partie sont connues, au plus tard pendant la partie. Le nom ne vaut que pour **un seul match** et il est oublié à la fin du match. Envoyé après la fin d'une partie, il s'applique à la partie suivante.
- Réponse : `"result": { "name": "Équipe 1 vs Équipe 2 - Match 3", "applied": true }`. `applied: false` signifie qu'il n'y a pas de match en cours ou que le match est terminé : le nom sera appliqué au début du match suivant.
- La sauvegarde automatique des replays (`rltm_auto_save_replays`) doit être activée côté plugin pour que le replay soit enregistré.

**Permissions** : l'utilisateur du plugin peut désactiver toutes les commandes (`rltm_server_control`), ou seulement certains groupes : caméra (`focus_player`), actions (`spectate`, `ready`, `hide_hud`), noms des replays (`set_replay_name`, `clear_replay_name`) et réglages (`set_setting`). Une commande désactivée est refusée avec `success: false` et une erreur du type `Command "focus_player" is disabled in the plugin settings` : affiche-la dans RLTM pour que l'opérateur sache quoi activer. `ping` et `resync` sont toujours autorisées.

Limites côté plugin : messages de 16 Ko maximum, 32 niveaux d'imbrication maximum, 64 commandes en attente maximum, et toute commande inconnue est refusée. Le joueur peut désactiver toutes les commandes (`rltm_server_control`). Gère donc toujours `success: false`.

Une idée de fonctionnalité si RLTM a une interface de régie : boutons « caméra sur ce joueur » (`focus_player`) et indicateur de connexion (`ping` régulier avec un `id`).

## Travail attendu

1. Adapter la réception des messages : nouveau topic `scoreboard`, objet `events`, renommage `hasFlip`, vérification du topic `plugin`.
2. Vider l'état local quand un payload d'état vaut `null` (fin de match).
3. Adapter les overlays et calculs de stats (tournois) qui lisaient `match.statistics` ou les compteurs à plat. Un compteur absent vaut 0.
4. **Enregistrer les résultats de tournoi depuis le topic `result`** (dédupliqué par `guid`), et associer chaque connexion à un match du tournoi grâce au nom du client (`plugin.client`). Si RLTM gère des jetons, vérifier l'en-tête `Authorization` pendant le handshake.
5. Utiliser `event` pour les animations (but avec buteur et vitesse, prolongation, podium) et `match.teams` pour les noms et couleurs quand ils sont réglés dans le jeu.
6. Exploiter `stats` pour les statistiques avancées (overlays et classements de tournoi).
7. Créer (ou adapter) un overlay « joueur regardé » piloté par le topic `spectator` : afficher les stats du `target` (depuis `scoreboard`), et le cacher quand `target` vaut `null`.
8. Utiliser `entities.time` pour l'interpolation là où des positions sont affichées (minimap, etc.).
9. Envoyer `set_replay_name` pour chaque partie d'une série, avec les noms d'équipes RLTM placés du bon côté (bleu/orange) et le numéro de la partie. Afficher une erreur dans RLTM si la réponse est `success: false`.
10. Garder `perMessageDeflate` activé sur le serveur WebSocket (c'est déjà le cas dans RLTM).
11. Optionnel : envoyer des commandes et gérer les réponses.
12. Mettre à jour les types, les tests et la documentation de RLTM. Si RLTM a des tests, ajoute des fixtures construites à partir des exemples ci-dessus.

À la fin, résume les fichiers modifiés, ce qui a été testé, et tout point du protocole qui t'a semblé ambigu ou manquant : il pourra être corrigé côté plugin.

# Configuration

Chaque réglage est disponible dans la fenêtre de réglages du plugin (++f2++ → *Plugins* → *RLTM Bridge*) et sous forme de cvar BakkesMod utilisable dans la console (++f6++), par exemple `rltm_hide_hud 0`.

Les réglages sont enregistrés dans `bakkesmod/cfg/rltm_bridge.cfg`, et peuvent être remis à leur valeur par défaut depuis la section *Reset* de la fenêtre.

## Fenêtre de réglages

| Section | Contenu |
|---|---|
| **Connexion** | URL du serveur, nom du client, jeton, connexion au chargement, bouton *Connect* / *Disconnect*, état de la connexion et dernière erreur |
| **Status** | Messages et kilo-octets envoyés, bouton *Resend the state*, commandes reçues et la dernière, nom du prochain replay |
| **Data sent to the server** | Données des balles et des voitures et leur intervalle d'envoi, statistiques avancées, joueur suivi par la caméra |
| **Spectator** | Rejoindre les spectateurs, masquer le HUD (et les éléments gardés), masquer les graphes de statistiques |
| **End of the match** | Se marquer prêt, sauvegarder le replay, format du nom du replay avec un aperçu, envoyer le replay au serveur |
| **Server control** | Commandes autorisées au serveur, par groupe |
| **Monitor** | Moniteur local pour la [page du moniteur](monitor.md), son port et son état, bouton pour ouvrir la page |
| **Advanced WebSocket settings** | Reconnexion, compression, délais, mode debug |
| **Reset** | Remettre tous les réglages par défaut |

## Connexion

| Cvar | Défaut | Description |
|---|---|---|
| `rltm_url` | `ws://localhost:3000` | URL du serveur WebSocket, `ws://` ou `wss://` (TLS, avec les certificats racine de Windows). La connexion redémarre quand elle change. |
| `rltm_client_name` | *(vide)* | Nom de ce client pour le serveur, le nom de l'ordinateur s'il est vide. Envoyé dans l'en-tête `X-RLTM-Client` et le message [`plugin`](protocol/messages.md#plugin). |
| `rltm_token` | *(vide)* | Jeton facultatif envoyé dans l'en-tête `Authorization: Bearer <jeton>`, pour que le serveur puisse refuser un client inconnu. Jamais envoyé dans les messages. |
| `rltm_auto_connect` | `0` | Se connecter au serveur au chargement du plugin |
| `rltm_auto_reconnect` | `1` | Se reconnecter automatiquement quand la connexion est perdue |
| `rltm_handshake_timeout` | `3` | Délai du handshake (secondes) |
| `rltm_ping_interval` | `1` | Intervalle des pings (secondes) |
| `rltm_min_wait_between_retries` | `500` | Attente minimale entre deux tentatives de reconnexion (millisecondes) |
| `rltm_max_wait_between_retries` | `500` | Attente maximale entre deux tentatives de reconnexion (millisecondes) |
| `rltm_per_message_deflate` | `1` | Compresser les messages (per message deflate), appliqué à la connexion suivante |
| `rltm_debug` | `0` | Écrire dans la console les messages envoyés au serveur et reçus de lui, sauf les entités et les statistiques avancées |

## Données envoyées au serveur {#data-sent-to-the-server}

Le [match](protocol/messages.md#match), le [tableau des scores](protocol/messages.md#scoreboard), les [joueurs](protocol/messages.md#players), les [événements](protocol/messages.md#event) et le [résultat](protocol/messages.md#result) sont toujours envoyés. Une donnée désactivée est effacée sur le serveur (son topic est envoyé avec un payload `null`).

| Cvar | Défaut | Description |
|---|---|---|
| `rltm_send_entities` | `1` | Envoyer les données des balles et des voitures ([`entities`](protocol/messages.md#entities)) |
| `rltm_entities_interval` | `100` | Intervalle entre deux envois des entités, de 16 à 1000 millisecondes |
| `rltm_send_stats` | `1` | Envoyer les statistiques avancées chaque seconde ([`stats`](protocol/messages.md#stats)), toujours incluses dans le résultat |
| `rltm_send_spectator` | `1` | Envoyer le joueur suivi par la caméra spectateur ([`spectator`](protocol/messages.md#spectator)) |

## Spectateur {#spectator}

| Cvar | Défaut | Description |
|---|---|---|
| `rltm_auto_spectate` | `1` | Rejoindre les spectateurs en rejoignant un match |
| `rltm_hide_hud` | `1` | Masquer le HUD du jeu en spectateur, pour afficher les overlays à la place. Le HUD revient dès que le réglage est désactivé. |
| `rltm_hud_names` | `1` | Garder les noms des joueurs au-dessus des voitures quand le HUD est masqué |
| `rltm_hud_match_info` | `1` | Garder les infos du match (score, temps) quand le HUD est masqué |
| `rltm_hide_stat_graphs` | `1` | Masquer les graphes de statistiques du moteur (++f10++) |

## Fin du match {#end-of-the-match}

| Cvar | Défaut | Description |
|---|---|---|
| `rltm_auto_ready` | `1` | Se marquer prêt à la fin du match |
| `rltm_auto_save_replays` | `1` | Sauvegarder le replay de chaque match. La valeur précédente du réglage de BakkesMod est restaurée au déchargement du plugin. |
| `rltm_upload_replays` | `1` | Envoyer le replay de chaque match au serveur une fois sauvegardé par le jeu (voir [Replays](protocol/index.md#replays)). Nécessite `rltm_auto_save_replays`. Matchs en ligne seulement. |
| `rltm_replays_folder` | | Dossier où le jeu sauvegarde les replays. Vide : `Documents\My Games\Rocket League\TAGame\Demos`. |
| `rltm_replay_name_format` | `{blue} vs {orange} - Match {game}` | Nom du replay quand le serveur envoie les équipes du match avec [`set_replay_name`](protocol/commands.md#set_replay_name) : `{blue}`, `{orange}` et `{game}` sont remplacés par l'équipe bleue, l'équipe orange et le numéro de la partie dans la série |

## Contrôle par le serveur {#server-control}

Le serveur peut envoyer des [commandes](protocol/commands.md) au jeu. Seules les commandes du plugin peuvent être exécutées, jamais des commandes de la console.

| Cvar | Défaut | Description |
|---|---|---|
| `rltm_server_control` | `1` | Autoriser le serveur à envoyer des commandes. Désactivé, toutes les commandes sont refusées. |
| `rltm_server_allow_camera` | `1` | Autoriser [`focus_player`](protocol/commands.md#focus_player) |
| `rltm_server_allow_actions` | `1` | Autoriser [`spectate`, `ready` et `hide_hud`](protocol/commands.md#spectate-ready-hide_hud) |
| `rltm_server_allow_replay_name` | `1` | Autoriser [`set_replay_name` et `clear_replay_name`](protocol/commands.md#set_replay_name) |
| `rltm_server_allow_settings` | `1` | Autoriser [`set_setting`](protocol/commands.md#set_setting) |

`ping` et `resync` sont toujours autorisées tant que le contrôle par le serveur est activé.

## Moniteur {#monitor}

Le [moniteur](monitor.md) affiche les messages du plugin et lui envoie des commandes depuis une page web, sans serveur.

| Cvar | Défaut | Description |
|---|---|---|
| `rltm_monitor` | `0` | Ouvrir un serveur WebSocket accessible uniquement depuis cet ordinateur (`127.0.0.1`) pour la page du moniteur. Seuls le site de la documentation et les pages locales peuvent s'y connecter. |
| `rltm_monitor_port` | `8765` | Port du moniteur, de 1024 à 65535 |

## Commandes de la console {#console-commands}

| Commande | Description |
|---|---|
| `rltm_connect` | Se connecter au serveur |
| `rltm_disconnect` | Se déconnecter du serveur |
| `rltm_version` | Afficher la version du plugin |

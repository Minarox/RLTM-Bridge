# Historique des versions

## 1.1.0 (protocole 2) {#110-protocol-2}

Le plugin est renommé **RLTM Bridge** (anciennement WSC, WebSocket Controller).

### Changements incompatibles {#breaking-changes}

- Les cvars et les commandes de la console sont préfixées par `rltm_` au lieu de `wsc_`, et enregistrées dans `rltm_bridge.cfg` : les anciens réglages ne sont pas repris.
- Les statistiques des joueurs passent de `match.statistics` au nouveau topic [`scoreboard`](protocol/messages.md#scoreboard), envoyé seulement quand elles changent.
- Les compteurs d'événements de stat (`Demolish`, `EpicSave`...) sont regroupés dans un objet `events` et ne contiennent que les événements reçus pour le joueur.
- `entities.cars[].asFlip` est renommé `hasFlip`.

### Nouveautés {#new}

- Message [`plugin`](protocol/messages.md#plugin) avec les versions du plugin et du protocole, et le nom du client.
- [`stats`](protocol/messages.md#stats) : statistiques avancées (boost, supersonique, temps en l'air, vitesse moyenne).
- [`event`](protocol/messages.md#event) : coup d'envoi, but (buteur, passeur, vitesse de la balle), prolongation, replays, fin du match, podium.
- [`result`](protocol/messages.md#result) : résultat complet de chaque match.
- [`spectator`](protocol/messages.md#spectator) : joueur suivi par la caméra spectateur.
- `match.guid`, `match.teams` (noms personnalisés et couleurs), `entities.time`.
- [Commandes](protocol/commands.md) du serveur, avec des messages [`response`](protocol/messages.md#response) : caméra, noms des replays, actions du joueur, réglages.
- Identification des clients : en-têtes `X-RLTM-Client` et `Authorization`.
- Fenêtre de réglages organisée par fonctionnalité, mode debug, réglages pour chaque action automatique.

### Corrections {#fixes}

- Plantages aléatoires : les entités étaient envoyées depuis un thread séparé partageant les données du thread du jeu.
- Un message du serveur contenant `{` pouvait faire planter le plugin.
- Les réglages de la connexion n'étaient appliqués que lorsqu'ils changeaient, et la connexion automatique ne fonctionnait pas.
- Deux événements de stat identiques à la suite (par exemple deux démolitions) n'étaient comptés qu'une fois.
- Le jeu pouvait ralentir quand le serveur ne lisait pas les messages assez vite.
- La sauvegarde automatique des replays de BakkesMod était désactivée au déchargement du plugin, elle est maintenant restaurée.

## 1.0.0 (protocole 1) {#100-protocol-1}

Première version : données du match, des statistiques, des joueurs et des entités, actions sur le HUD et en spectateur.

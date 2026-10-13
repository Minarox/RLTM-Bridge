# Checklist de test en jeu

Tout ce qu'il faut vérifier dans Rocket League avant de fusionner `dev` dans `main`. Le plugin n'a été vérifié que par une analyse syntaxique (clang + SDK BakkesMod) et des tests unitaires : **rien n'a encore tourné dans le jeu**.

Les points marqués ⚠️ reposent sur une **hypothèse sur le jeu ou le SDK** que je n'ai pas pu vérifier. Ce sont les plus importants : note précisément ce que tu observes, car ils peuvent demander une correction.

Coche au fur et à mesure (`[x]`), et note les écarts sous chaque point.

---

## 0. Préparation

- [ ] Activer les GitHub Actions (*Settings → Actions → General → Allow all actions*), puis pousser un commit (ou relancer le dernier run) : les jobs **test** et **build** doivent être verts, et l'artefact `WSCPlugin` doit contenir `WSCPlugin.dll`.
- [ ] Ou compiler en local : Visual Studio 2022, `WSC.sln`, configuration `Release | x64`. Aucune erreur, la DLL est dans `plugins/`.
- [ ] Installer la DLL dans BakkesMod (le build local le fait via le post-build, sinon copier `WSCPlugin.dll` dans `bakkesmod/plugins/` et `plugin load wscplugin` dans la console F6).
- [ ] Lancer le serveur de test : `pip install websockets` puis `python tools/test_server.py` (tape `help` pour les commandes). Il affiche tout ce que le plugin envoie, et résume les `entities`/`stats` toutes les 5 s (`--all` pour tout voir).
- [ ] Activer le mode debug pendant les tests : `wsc_debug 1` dans la console F6 (ou *Advanced WebSocket settings → Debug mode*). La console affiche alors chaque message envoyé et reçu.

## 1. Chargement et connexion

- [ ] La console affiche `WSC plugin v1.0.0.x loaded`, sans erreur.
- [ ] F2 → Plugins → WebSocket Controller : la fenêtre s'affiche avec les sections *Status*, *Data sent to the server*, *Spectator*, *End of the match*, *Server control*, *Advanced WebSocket settings*, *Reset*.
- [ ] Bouton **Connect** : l'état passe à **Connected** (vert) avec l'heure. Le serveur de test affiche `Client connected: <nom du PC> (Authorization: no, subprotocol: json)`, puis le message `plugin` avec `version`, `protocol: 2` et `client`.
- [ ] **Client name** : saisir `PC-TEST` puis cliquer ailleurs : le plugin se reconnecte et le serveur affiche `Client connected: PC-TEST`.
- [ ] **Token** : relancer le serveur avec `--token SECRET`. Sans jeton → le serveur refuse (`Rejected client`), la console affiche `WebSocket connection closed: Invalid token (4001)` et le plugin retente régulièrement. Saisir `SECRET` dans *Token* (les caractères doivent être masqués) → connexion acceptée (`Authorization: yes`).
- [ ] Arrêter le serveur (Ctrl+C) : l'état passe à **Reconnecting...**. Le relancer : le plugin se reconnecte tout seul et renvoie l'état (`plugin`, puis `match`, `scoreboard`, etc. si un match est en cours).
- [ ] Erreur répétée : serveur arrêté, la console ne doit afficher l'erreur qu'**une fois**, pas à chaque tentative.
- [ ] **Connect when the plugin is loaded** coché + redémarrage du jeu : connexion automatique au lancement.
- [ ] Changer l'URL pendant que c'est connecté (ex. port 3001 avec `--port 3001`) : reconnexion sur la nouvelle URL.
- [ ] (Optionnel) `wss://` vers un serveur TLS public : la connexion s'établit (certificats Windows).
- [ ] **Disconnect** : l'état passe à **Disconnected** (gris), plus de reconnexion.

## 2. Réglages

- [ ] Chaque case à cocher change la cvar correspondante (vérifier dans F6, ex. `wsc_hide_hud`) et **reste après un redémarrage du jeu** (fichier `bakkesmod/cfg/wsc.cfg`).
- [ ] Les sous-options n'apparaissent que si l'option parente est cochée (intervalle des entités, éléments du HUD, nom du replay, groupes de commandes).
- [ ] Curseur **Update interval** : la valeur n'est appliquée qu'au relâchement (pas de saccade du jeu pendant le glissement), entre 16 et 1000 ms.
- [ ] **Replay name** : l'aperçu suit le format saisi (ex. `{game} - {blue} vs {orange}` → `1 - Blue Team vs Orange Team`).
- [ ] **Reset → Reset all the settings** : une fenêtre de confirmation s'ouvre ; *Cancel* ne change rien, *Reset* remet toutes les valeurs par défaut.
- [ ] *Status* : le compteur de messages et de Ko augmente pendant un match, *Commands received* augmente quand le serveur envoie une commande.

## 3. Match privé : données de base

Créer une partie privée (avec un deuxième compte ou des amis), le PC du plugin en spectateur. À défaut, une partie hors ligne avec des bots fonctionne pour la plupart des points (le `guid` vaudra alors `null`).

- [ ] **Join the spectators** : en rejoignant la partie, le joueur passe automatiquement en spectateur.
- [ ] `match` : `clock` diminue chaque seconde, `score` suit les buts, `isStarted` passe à `true` au premier coup d'envoi, `isPaused` suit la pause, `isOvertime` la prolongation, `isEnded` la fin, `isReplay` les replays de but.
- [ ] `match.guid` : une chaîne non vide en partie privée en ligne ⚠️ (format du GUID à noter).
- [ ] `match.teams` : sans nom personnalisé, `name` vaut `null`. Régler des noms et des couleurs d'équipe dans les paramètres de la partie privée ⚠️ : `name` doit les reprendre, et les couleurs `primaryColor`/`secondaryColor` doivent ressembler aux couleurs du jeu (colle les codes `#RRGGBB` dans un sélecteur de couleur). **Si elles sont plus sombres que dans le jeu, signale-le** : ce serait un problème de conversion d'espace colorimétrique.
- [ ] `players` : tous les joueurs avec `teamIndex` (0 bleu, 1 orange, 255 spectateur) et `carId`.
- [ ] Quitter le match : `match`, `scoreboard`, `stats`, `spectator`, `entities` et `players` passent à `null`. `result` ne change pas.

## 4. Statistiques et doublons ⚠️

Le plugin suppose que le jeu envoie **deux fois** chaque événement de stat (« stat ticker ») et ignore la copie reçue dans les 100 ms.

- [ ] Marquer **un** but : `scoreboard.<joueur>.events.Goal` vaut **1** (pas 2), et le serveur affiche **un seul** message `statistic` « Goal » pour ce joueur. Si tu vois 2, le filtre ne fonctionne pas.
- [ ] Faire **deux démolitions d'affilée** avec le même joueur (sans autre événement entre les deux) : `events.Demolish` vaut **2**.
- [ ] Les compteurs du jeu (`score`, `goals`, `shots`, `assists`, `saves`, `ballTouches`) correspondent au tableau des scores du jeu.

## 5. Statistiques avancées (`stats`)

- [ ] Un message `stats` par seconde pendant le jeu (seulement quand une valeur change : aucun pendant la pause, le compte à rebours ou un replay de but, où rien n'est compté).
- [ ] `boostUsed` augmente quand un joueur utilise du boost, pas quand il en ramasse. Après une démolition, le boost du nouveau véhicule (33) n'est pas compté, et le boost utilisé par les voitures **pendant le replay d'un but** non plus.
- [ ] `boostPickups` augmente à chaque ramassage de pastille (compteur du jeu).
- [ ] `timeSupersonic` augmente seulement en supersonique, `timeInAir` pendant les sauts et aériens, `timeOnWall` sur les murs, `timeOnGround` le reste.
- [ ] `timeInPlay` ≈ temps de jeu réel, **sans** les comptes à rebours ni les replays de but. ⚠️ Repose sur l'événement « premier contact après le coup d'envoi » (`OnBallHasBeenHit`) : si `timeInPlay` reste à 0 après le premier but, cet événement n'arrive pas à chaque coup d'envoi.
- [ ] `averageSpeed` est plausible (de l'ordre de 50 à 80 km/h).
- [ ] Décocher **Send the advanced statistics** : `stats` passe à `null`, mais le `result` les contient toujours.

## 6. Événements (`event`)

- [ ] `kickoff` à chaque coup d'envoi (début du compte à rebours).
- [ ] `goal` ~0,5 s après chaque but ⚠️ (événement `Ball_TA.OnHitGoal`) :
  - [ ] `scorer` est le bon joueur ;
  - [ ] `assister` est le passeur, ou `null` sans passe ;
  - [ ] `teamIndex` est l'équipe qui marque (bleu = 0). ⚠️ Sans buteur, l'équipe est déduite du côté du but (but orange côté Y positif) ;
  - [ ] `ballSpeed` est plausible (comparer avec la vitesse affichée par le jeu si elle l'est) ;
  - [ ] **But contre son camp** : `isOwnGoal: true`, `scorer` est le joueur qui l'a marqué et `teamIndex` l'équipe adverse.
- [ ] `overtime` au début de la prolongation.
- [ ] `replayStart` / `replayEnd` autour de chaque replay de but.
- [ ] `matchEnded` à la fin, avec `winner` et `score`.
- [ ] `podium` au début de l'écran du podium ⚠️ (événement `PodiumSpotlight.BeginState`).

## 7. Résultat (`result`)

- [ ] À la fin du match, un seul `result`, **après** le `goal` du dernier but (tester aussi un but en or en prolongation).
- [ ] `winner` correct, `mvp` correct (comparer avec l'écran de fin), `score`, `duration`, `timePlayed` (prolongation comprise), `isOvertime`, `teams`, `scoreboard` et `stats` complets.
- [ ] `replayName` contient le nom donné par `set_replay_name` (sinon `null`).
- [ ] Arrêter puis relancer le serveur **après** la fin du match (même après avoir quitté le match) : le `result` est renvoyé à la reconnexion.
- [ ] Il reste le même pendant le match suivant, et n'est remplacé qu'à la fin de celui-ci.

## 8. Caméra spectateur (`spectator`) ⚠️

- [ ] Suivre un joueur (clic ou touches de changement de joueur) : `target` contient ce joueur.
- [ ] Passer à un autre joueur : `target` change.
- [ ] Repasser en **caméra libre** : `target` vaut `null`. ⚠️ Repose sur le PRI de la cible de la caméra : si `target` garde le dernier joueur, signale-le.
- [ ] Noter les valeurs de `camera` pour chaque mode (libre, joueur, balle…) : elles serviront à documenter ce champ.
- [ ] Vérifier qu'il n'y a pas de rafale de messages `spectator` pendant les transitions de caméra (le résumé du serveur ne les compte pas, regarder la console en mode debug).

## 9. Entités (`entities`)

- [ ] Environ 10 messages par seconde pendant le jeu (« 50 messages in 5 s »), aucun pendant la pause et après la fin.
- [ ] `time` augmente d'environ 100 entre deux messages (lancer avec `--all` pour voir chaque message).
- [ ] `cars[].hasFlip`, `boost`, `isSuperSonic`, `isOnGround`, `isOnWall` cohérents avec le jeu ; `balls[].speed` plausible.
- [ ] Curseur à 16 ms : ~60 messages par seconde, sans baisse de FPS.
- [ ] Décocher **Send the balls and cars data** : `entities` passe à `null`, plus aucun message.
- [ ] (Optionnel) Serveur qui ne lit plus : suspendre le processus du serveur (Ctrl+Z sous Linux/WSL, ou mettre le terminal en pause avec un clic dans la fenêtre sous Windows) pendant un match. La console doit afficher `entities updates are skipped`, **sans saccade du jeu**, puis `resumed` à la reprise.

## 10. HUD et actions automatiques

- [ ] **Hide the HUD** : en spectateur, le HUD disparaît ; noms des joueurs et infos du match visibles selon *Keep the players names* / *Keep the match info*.
- [ ] Décocher **Hide the HUD** pendant le match : le HUD revient immédiatement.
- [ ] **Hide the statistics graphs** : les graphes F10 ne s'affichent pas ⚠️ (niveau 6 : vérifier qu'il masque bien les graphes).
- [ ] **Mark as ready** : à la fin du match, le plugin se marque prêt automatiquement.
- [ ] **Save the replay** : le replay est sauvegardé automatiquement à la fin de chaque match. Au déchargement du plugin (`plugin unload wscplugin`), `ranked_autosavereplay_all` reprend sa valeur d'avant.

## 11. Nom des replays ⚠️

- [ ] Pendant un match : `replay Équipe_1 Équipe_2 3` dans le serveur de test → réponse `success: true`, `result.name: "Équipe 1 vs Équipe 2 - Match 3"`, `applied: true`. La fenêtre affiche *Next replay name*.
- [ ] À la fin du match, **le replay sauvegardé porte ce nom dans la liste des replays du jeu** ⚠️ (le jeu pourrait écraser le nom à la sauvegarde automatique : dans ce cas, il faudra exporter le replay soi-même).
- [ ] Accents et caractères spéciaux affichés correctement dans le jeu.
- [ ] Commande envoyée **après** la fin d'un match : `applied: false`, le nom est utilisé pour le match **suivant** (pas pour celui qui vient de finir).
- [ ] Bouton **Clear** de la fenêtre (ou `clear_replay`) : le match suivant garde le nom par défaut du jeu.
- [ ] `replayname Nom complet` : le nom est utilisé tel quel.

## 12. Commandes du serveur et robustesse ⚠️

Chaque commande doit recevoir un `response`. **Aucune ne doit faire planter ou saccader le jeu.**

- [ ] `ping` → `success: true`.
- [ ] `resync` → tous les états sont renvoyés.
- [ ] `spectate`, `ready`, `hide_hud` → effet visible dans le jeu, `success: true` (et une erreur claire hors match).
- [ ] `focus <nom d'un joueur>` → la caméra suit ce joueur ⚠️ (`SetFocusActor` avec le pseudo). Avec un nom inconnu → `success: false`.
- [ ] `set wsc_hide_hud 0` → réglage appliqué ; `set wsc_url ws://x` → refusé (réglage de connexion).
- [ ] Décocher chaque groupe dans *Server control* → les commandes du groupe sont refusées avec `is disabled in the plugin settings`. Décocher *Allow the server to send commands* → tout est refusé.
- [ ] `bad` (JSON invalide), `unknown` (commande inconnue), `deep` (JSON trop profond), `big` (message de 20 Ko) → réponses d'erreur, aucun crash.
- [ ] `flood` (200 commandes d'un coup) → environ 64 réponses `success`, les autres `Too many pending commands`, **sans saccade**.

## 13. Stabilité

- [ ] Décharger/recharger le plugin **pendant un match** (`plugin unload wscplugin` puis `plugin load wscplugin`) : aucun crash, les données reprennent.
- [ ] Alt-Tab pendant un match, puis retour : pas de valeurs aberrantes dans `stats` (les images de plus de 0,1 s ne sont pas comptées).
- [ ] Enchaîner plusieurs matchs : chaque match repart de zéro (`scoreboard`, `stats`), un `result` par match.
- [ ] Un match complet avec le debug désactivé : FPS stables, pas d'augmentation continue de la mémoire du jeu.
- [ ] Fermer le jeu pendant que c'est connecté : pas de crash à la fermeture.

---

## À me remonter

Pour chaque point qui ne se comporte pas comme prévu : ce que tu as fait, ce que tu attendais, ce que tu as vu, et si possible les lignes de la console F6 (mode debug) ou du serveur de test.

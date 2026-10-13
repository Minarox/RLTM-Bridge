# Intégration serveur

Cette page montre comment recevoir les données du plugin dans un serveur WebSocket. Les exemples utilisent [Bun](https://bun.sh) et [Elysia](https://elysiajs.com), comme RLTM, mais n'importe quel serveur WebSocket convient : le plugin a seulement besoin d'un point d'accès WebSocket qui accepte des messages texte JSON.

## Serveur minimal {#minimal-server}

```ts title="server.ts"
import { Elysia } from "elysia";

// Dernière valeur de chaque topic d'état, envoyée aux overlays
const state: Record<string, unknown> = {};
const results = new Map<string, unknown>();

new Elysia()
    .ws("/", {
        // Compression proposée par le plugin
        perMessageDeflate: true,

        // Facultatif : refuser les clients inconnus pendant le handshake
        beforeHandle({ request, set }) {
            const token = request.headers.get("authorization");
            if (process.env.TOKEN && token !== `Bearer ${process.env.TOKEN}`) {
                set.status = 401;
                return "Unauthorized";
            }
        },

        open(ws) {
            console.log("Plugin connecté :", ws.data.headers["x-rltm-client"]);
        },

        message(ws, message: any) {
            const { topic, payload } = message;

            switch (topic) {
                case "plugin":
                    if (payload.protocol !== 2) console.warn("Protocole non supporté", payload.protocol);
                    break;

                case "result": {
                    // Renvoyé à chaque connexion : le dédupliquer
                    const key = payload?.guid ?? `${payload?.client}-${payload?.endedAt}`;
                    if (payload && !results.has(key)) results.set(key, payload);
                    state.result = payload;
                    break;
                }

                case "statistic":
                case "event":
                    // Messages ponctuels : déclencher les animations des overlays
                    console.log(topic, payload);
                    break;

                case "response":
                    if (!payload.success) console.warn(`La commande ${payload.command} a échoué : ${payload.error}`);
                    break;

                default:
                    // Topics d'état : le payload remplace le précédent, null l'efface
                    state[topic] = payload;
            }
        },

        close() {
            console.log("Plugin déconnecté");
        }
    })
    .listen(3000);
```

Connecter le plugin à `ws://localhost:3000`.

## Envoyer des commandes {#sending-commands}

```ts
// Centrer la caméra sur un joueur
ws.send(JSON.stringify({ command: "focus_player", payload: { name: "Player1" }, id: "camera-1" }));

// Nommer le replay de la partie en cours de la série
ws.send(JSON.stringify({ command: "set_replay_name", payload: { blue: "Équipe 1", orange: "Équipe 2", game: 3 } }));
```

Associez les réponses grâce à l'`id` dans le topic [`response`](protocol/messages.md#response).

## Liste de vérification {#integration-checklist}

- [ ] Vérifier `protocol` dans le message [`plugin`](protocol/messages.md#plugin).
- [ ] Garder la dernière valeur de chaque topic d'**état**, et l'effacer sur un payload `null`.
- [ ] Ne pas attendre qu'un état soit renvoyé s'il n'a pas changé : le garder.
- [ ] **Dédupliquer le [`result`](protocol/messages.md#result)** avec son `guid` : il est renvoyé à chaque connexion.
- [ ] Lire les compteurs `events` absents du [`scoreboard`](protocol/messages.md#scoreboard) comme `0`.
- [ ] Interpoler les positions des [`entities`](protocol/messages.md#entities) avec leur `time`, sans supposer un intervalle fixe.
- [ ] Utiliser l'en-tête `X-RLTM-Client` (ou `plugin.client`) pour distinguer les ordinateurs quand plusieurs matchs se jouent en même temps.
- [ ] Gérer les [réponses](protocol/messages.md#response) `success: false` : une commande peut être désactivée par l'utilisateur du plugin.
- [ ] Ignorer les topics et les champs inconnus : de nouveaux peuvent être ajoutés sans changer la version du protocole.
- [ ] N'envoyer au plugin que des [commandes](protocol/commands.md) : tout autre message reçoit une erreur en réponse.

## Tester {#testing}

Le dépôt contient un serveur de test, en Python, qui affiche chaque message du plugin et envoie les commandes tapées dans le terminal (des valides, et des invalides pour vérifier la robustesse du plugin) :

```bash
pip install websockets
python tools/test_server.py --port 3000 --token SECRET
```

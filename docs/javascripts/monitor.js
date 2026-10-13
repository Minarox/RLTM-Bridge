// Monitor page: connects to the local monitor of the plugin (ws://127.0.0.1:<port>), shows the messages and sends commands.
// The data of the game (player names...) is always inserted as text, never as HTML.
(function () {
    "use strict";

    const TEXTS = {
        en: {
            port: "Port", connect: "Connect", disconnect: "Disconnect", autoReconnect: "Reconnect automatically",
            disconnected: "Disconnected", connecting: "Connecting...", connected: "Connected",
            connectionHelp: "Can't connect: enable the monitor in the plugin (F2 → Plugins → RLTM Bridge → Monitor) and check the port. The browser may ask to allow the access to the local network.",
            plugin: "Plugin", match: "Match", camera: "Camera", traffic: "Traffic",
            noData: "No data", noMatch: "No match", freeCamera: "Free camera",
            started: "Started", paused: "Paused", overtime: "Overtime", ended: "Ended", replay: "Replay",
            messagesPerSecond: "messages/s", entitiesPerSecond: "entities/s", received: "received",
            commands: "Commands", focus: "Focus the camera", player: "Player", replayName: "Replay name",
            blue: "Blue team", orange: "Orange team", game: "Game", setName: "Set", clear: "Clear",
            raw: "Raw command (JSON)", send: "Send",
            states: "States", updated: "updated", log: "Messages", pause: "Pause", resume: "Resume", clearLog: "Clear",
            notConnected: "Not connected to the monitor", invalidJson: "Invalid JSON"
        },
        fr: {
            port: "Port", connect: "Connecter", disconnect: "Déconnecter", autoReconnect: "Reconnexion automatique",
            disconnected: "Déconnecté", connecting: "Connexion...", connected: "Connecté",
            connectionHelp: "Connexion impossible : activez le moniteur dans le plugin (F2 → Plugins → RLTM Bridge → Monitor) et vérifiez le port. Le navigateur peut demander d'autoriser l'accès au réseau local.",
            plugin: "Plugin", match: "Match", camera: "Caméra", traffic: "Trafic",
            noData: "Aucune donnée", noMatch: "Aucun match", freeCamera: "Caméra libre",
            started: "Commencé", paused: "En pause", overtime: "Prolongation", ended: "Terminé", replay: "Replay",
            messagesPerSecond: "messages/s", entitiesPerSecond: "entités/s", received: "reçus",
            commands: "Commandes", focus: "Centrer la caméra", player: "Joueur", replayName: "Nom du replay",
            blue: "Équipe bleue", orange: "Équipe orange", game: "Partie", setName: "Appliquer", clear: "Effacer",
            raw: "Commande brute (JSON)", send: "Envoyer",
            states: "États", updated: "mis à jour", log: "Messages", pause: "Pause", resume: "Reprendre", clearLog: "Vider",
            notConnected: "Non connecté au moniteur", invalidJson: "JSON invalide"
        }
    };

    const STATE_TOPICS = ["match", "scoreboard", "stats", "spectator", "players", "entities", "result"];
    const LOG_TOPICS = ["plugin", "match", "scoreboard", "stats", "spectator", "players", "entities", "statistic", "event", "result", "response", "command"];
    const HIDDEN_BY_DEFAULT = ["entities", "stats"];
    const MAX_LOG_ENTRIES = 300;
    const RECONNECT_DELAY = 2000;

    function element(tag, attributes, children) {
        const node = document.createElement(tag);
        for (const [key, value] of Object.entries(attributes || {})) {
            if (key === "text") node.textContent = value;
            else if (key === "class") node.className = value;
            else if (key.startsWith("on")) node.addEventListener(key.slice(2), value);
            else node.setAttribute(key, value);
        }
        for (const child of children || []) node.append(child);
        return node;
    }

    function time(date) {
        return date.toLocaleTimeString([], { hour12: false });
    }

    function formatClock(seconds) {
        if (typeof seconds !== "number") return "-";
        const value = Math.max(0, Math.round(seconds));
        return `${Math.floor(value / 60)}:${String(value % 60).padStart(2, "0")}`;
    }

    function init(root) {
        const t = TEXTS[root.dataset.lang] || TEXTS.en;
        const params = new URLSearchParams(window.location.search);

        let socket = null;
        let wanted = false;
        let reconnectTimer = null;
        let commandId = 0;
        let paused = false;
        const states = {};
        const counters = { messages: 0, entities: 0, total: 0 };
        const rates = { messages: 0, entities: 0 };

        // --- Toolbar ---
        const portInput = element("input", { type: "number", min: "1024", max: "65535", value: params.get("port") || "8765", class: "rltm-monitor__port" });
        const connectButton = element("button", { class: "md-button md-button--primary", text: t.connect });
        const autoReconnect = element("input", { type: "checkbox", checked: "" });
        const status = element("span", { class: "rltm-monitor__status", text: t.disconnected });
        const help = element("p", { class: "rltm-monitor__help", hidden: "" , text: t.connectionHelp });

        const toolbar = element("div", { class: "rltm-monitor__toolbar" }, [
            element("label", {}, [t.port + " ", portInput]),
            connectButton,
            element("label", {}, [autoReconnect, " " + t.autoReconnect]),
            status
        ]);

        // --- Summary cards ---
        const pluginCard = element("div", { class: "rltm-monitor__value", text: t.noData });
        const matchCard = element("div", { class: "rltm-monitor__value", text: t.noMatch });
        const cameraCard = element("div", { class: "rltm-monitor__value", text: t.noData });
        const trafficCard = element("div", { class: "rltm-monitor__value", text: "-" });

        const card = (title, content) => element("div", { class: "rltm-monitor__card" }, [element("div", { class: "rltm-monitor__label", text: title }), content]);
        const cards = element("div", { class: "rltm-monitor__cards" }, [card(t.plugin, pluginCard), card(t.match, matchCard), card(t.camera, cameraCard), card(t.traffic, trafficCard)]);

        // --- Commands ---
        const playerSelect = element("select", { class: "rltm-monitor__input" });
        const blueInput = element("input", { class: "rltm-monitor__input", placeholder: t.blue });
        const orangeInput = element("input", { class: "rltm-monitor__input", placeholder: t.orange });
        const gameInput = element("input", { class: "rltm-monitor__input rltm-monitor__game", type: "number", min: "1", max: "99", value: "1", title: t.game });
        const rawInput = element("textarea", { class: "rltm-monitor__raw", rows: "3", text: '{ "command": "ping" }' });

        const button = (label, action) => element("button", { class: "md-button", text: label, onclick: action });
        const commands = element("div", { class: "rltm-monitor__panel" }, [
            element("h2", { text: t.commands }),
            element("div", { class: "rltm-monitor__row" }, [
                button("ping", () => sendCommand("ping")),
                button("resync", () => sendCommand("resync")),
                button("spectate", () => sendCommand("spectate")),
                button("ready", () => sendCommand("ready")),
                button("hide_hud", () => sendCommand("hide_hud"))
            ]),
            element("div", { class: "rltm-monitor__row" }, [
                element("span", { class: "rltm-monitor__label", text: t.focus }),
                playerSelect,
                button("focus_player", () => {
                    const player = playerSelect.value ? JSON.parse(playerSelect.value) : null;
                    if (player) sendCommand("focus_player", { uid: player.uid, name: player.name });
                })
            ]),
            element("div", { class: "rltm-monitor__row" }, [
                element("span", { class: "rltm-monitor__label", text: t.replayName }),
                blueInput, orangeInput, gameInput,
                button(t.setName, () => sendCommand("set_replay_name", { blue: blueInput.value, orange: orangeInput.value, game: Number(gameInput.value) })),
                button(t.clear, () => sendCommand("clear_replay_name"))
            ]),
            element("div", { class: "rltm-monitor__label", text: t.raw }),
            rawInput,
            element("div", { class: "rltm-monitor__row" }, [button(t.send, () => sendRaw(rawInput.value))])
        ]);

        // --- States ---
        const stateViews = {};
        const statesList = element("div");
        for (const topic of STATE_TOPICS) {
            const summary = element("summary", { text: `${topic} — ${t.noData}` });
            const content = element("pre", { class: "rltm-monitor__json" });
            const details = element("details", { class: "rltm-monitor__state" }, [summary, content]);
            details.addEventListener("toggle", () => { if (details.open) content.textContent = JSON.stringify(states[topic]?.payload ?? null, null, 2); });
            stateViews[topic] = { summary, content, details };
            statesList.append(details);
        }
        const statesPanel = element("div", { class: "rltm-monitor__panel" }, [element("h2", { text: t.states }), statesList]);

        // --- Log ---
        const filters = {};
        const filterRow = element("div", { class: "rltm-monitor__row rltm-monitor__filters" });
        for (const topic of LOG_TOPICS) {
            const checkbox = element("input", { type: "checkbox" });
            checkbox.checked = !HIDDEN_BY_DEFAULT.includes(topic);
            filters[topic] = checkbox;
            filterRow.append(element("label", {}, [checkbox, " " + topic]));
        }
        const pauseButton = button(t.pause, () => { paused = !paused; pauseButton.textContent = paused ? t.resume : t.pause; });
        const logList = element("div", { class: "rltm-monitor__log" });
        const logPanel = element("div", { class: "rltm-monitor__panel" }, [
            element("h2", { text: t.log }),
            filterRow,
            element("div", { class: "rltm-monitor__row" }, [pauseButton, button(t.clearLog, () => logList.replaceChildren())]),
            logList
        ]);

        root.replaceChildren(toolbar, help, cards, commands, statesPanel, logPanel);

        // --- Rendering ---
        function setStatus(text, className) {
            status.textContent = text;
            status.className = "rltm-monitor__status " + (className || "");
        }

        function addLog(direction, topic, payload) {
            if (paused || !filters[topic]?.checked) return;

            const text = JSON.stringify(payload);
            const entry = element("details", { class: "rltm-monitor__entry" }, [
                element("summary", {}, [
                    element("span", { class: "rltm-monitor__time", text: time(new Date()) }),
                    element("span", { class: "rltm-monitor__direction", text: direction }),
                    element("span", { class: "rltm-monitor__topic", text: topic }),
                    element("span", { class: "rltm-monitor__preview", text: text && text.length > 160 ? text.slice(0, 160) + "…" : String(text) })
                ]),
                element("pre", { class: "rltm-monitor__json", text: JSON.stringify(payload, null, 2) })
            ]);
            logList.prepend(entry);
            while (logList.childElementCount > MAX_LOG_ENTRIES) logList.lastElementChild.remove();
        }

        function renderPlugin(payload) {
            pluginCard.textContent = payload ? `v${payload.version} · protocol ${payload.protocol} · ${payload.client}` : t.noData;
        }

        function renderMatch(match) {
            if (!match) {
                matchCard.textContent = t.noMatch;
                return;
            }
            const flags = [
                match.isStarted && t.started, match.isPaused && t.paused, match.isOvertime && t.overtime,
                match.isEnded && t.ended, match.isReplay && t.replay
            ].filter(Boolean);
            const score = Array.isArray(match.score) ? match.score : [];
            matchCard.replaceChildren(
                element("span", { class: "rltm-monitor__blue", text: String(score[0] ?? "-") }),
                " - ",
                element("span", { class: "rltm-monitor__orange", text: String(score[1] ?? "-") }),
                `  ${formatClock(match.clock)}  ${match.arenaCode || ""}`,
                element("div", { class: "rltm-monitor__flags", text: flags.join(" · ") })
            );
        }

        function renderCamera(spectator) {
            if (!spectator) cameraCard.textContent = t.noData;
            else if (!spectator.target) cameraCard.textContent = t.freeCamera;
            else cameraCard.textContent = `${spectator.target.name} (${spectator.target.teamIndex === 0 ? t.blue : t.orange})`;
        }

        function renderPlayers(players) {
            const selected = playerSelect.value;
            playerSelect.replaceChildren();
            for (const player of Array.isArray(players) ? players : []) {
                if (player.teamIndex === 255) continue;
                const value = JSON.stringify({ uid: player.uid, name: player.name });
                const option = element("option", { value, text: `${player.name} (${player.teamIndex === 0 ? t.blue : t.orange})` });
                if (value === selected) option.selected = true;
                playerSelect.append(option);
            }
        }

        function renderState(topic, payload) {
            const state = states[topic] = { payload, count: (states[topic]?.count || 0) + 1, time: new Date() };
            const view = stateViews[topic];
            view.summary.textContent = `${topic} — ${payload === null ? "null" : t.updated} ${time(state.time)} (${state.count})`;
            if (view.details.open) view.content.textContent = JSON.stringify(payload, null, 2);

            if (topic === "match") renderMatch(payload);
            if (topic === "spectator") renderCamera(payload);
            if (topic === "players") renderPlayers(payload);
        }

        function onMessage(data) {
            let message;
            try {
                message = JSON.parse(data);
            } catch {
                addLog("←", "response", { error: t.invalidJson, data });
                return;
            }

            const { topic, payload } = message;
            counters.messages++;
            counters.total++;
            if (topic === "entities") counters.entities++;

            if (topic === "plugin") renderPlugin(payload);
            if (STATE_TOPICS.includes(topic)) renderState(topic, payload);
            addLog("←", topic, payload);
        }

        setInterval(() => {
            rates.messages = counters.messages;
            rates.entities = counters.entities;
            counters.messages = 0;
            counters.entities = 0;
            trafficCard.textContent = `${rates.messages} ${t.messagesPerSecond} · ${rates.entities} ${t.entitiesPerSecond} · ${counters.total} ${t.received}`;
        }, 1000);

        // --- Connection ---
        function connect() {
            clearTimeout(reconnectTimer);
            if (socket) socket.close();

            wanted = true;
            connectButton.textContent = t.disconnect;
            setStatus(t.connecting, "rltm-monitor__status--pending");

            socket = new WebSocket(`ws://127.0.0.1:${Number(portInput.value) || 8765}`);
            socket.addEventListener("open", () => {
                help.hidden = true;
                setStatus(t.connected, "rltm-monitor__status--ok");
            });
            socket.addEventListener("message", (event) => onMessage(event.data));
            socket.addEventListener("close", () => {
                socket = null;
                if (!wanted) return;

                help.hidden = false;
                setStatus(t.disconnected, "rltm-monitor__status--error");
                if (autoReconnect.checked) reconnectTimer = setTimeout(connect, RECONNECT_DELAY);
            });
        }

        function disconnect() {
            wanted = false;
            clearTimeout(reconnectTimer);
            if (socket) socket.close();
            socket = null;
            connectButton.textContent = t.connect;
            setStatus(t.disconnected);
        }

        function sendRaw(text) {
            if (!socket || socket.readyState !== WebSocket.OPEN) {
                addLog("→", "command", { error: t.notConnected });
                return;
            }
            try {
                addLog("→", "command", JSON.parse(text));
            } catch {
                addLog("→", "command", { error: t.invalidJson, text });
            }
            socket.send(text);
        }

        function sendCommand(command, payload) {
            sendRaw(JSON.stringify({ command, payload: payload || {}, id: `monitor-${++commandId}` }));
        }

        connectButton.addEventListener("click", () => (wanted ? disconnect() : connect()));
        connect();
    }

    function start() {
        const root = document.getElementById("rltm-monitor");
        if (root && !root.dataset.started) {
            root.dataset.started = "true";
            init(root);
        }
    }

    if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start);
    else start();
})();

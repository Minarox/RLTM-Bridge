---
hide:
  - toc
---

# Moniteur

Affiche tout ce que le plugin envoie et lui envoie des commandes, depuis cette page, sans serveur : pour vérifier les données d'un match, ou pour essayer les commandes.

1. Dans Rocket League : ++f2++ → *Plugins* → *RLTM Bridge* → *Monitor* → cocher **Enable the monitor**. Le plugin ouvre un serveur WebSocket accessible uniquement depuis cet ordinateur (`ws://127.0.0.1:8765`). Le bouton *Open the monitor page* ouvre cette page avec le bon port.
2. Cliquer sur **Connecter** ci-dessous. L'état actuel du jeu est reçu immédiatement, puis chaque message au moment où il est envoyé.

Le moniteur reçoit les mêmes messages que le serveur, même quand le plugin n'est connecté à aucun serveur, et les commandes suivent les mêmes règles et [permissions](configuration.md#server-control).

!!! note
    Seuls ce site et les pages locales (`localhost`, `127.0.0.1`, fichiers) peuvent se connecter au moniteur du plugin. Le navigateur peut demander d'autoriser cette page à accéder aux appareils du réseau local : acceptez.

<div id="rltm-monitor" class="rltm-monitor" data-lang="fr" markdown>
*Le moniteur nécessite JavaScript.*
</div>

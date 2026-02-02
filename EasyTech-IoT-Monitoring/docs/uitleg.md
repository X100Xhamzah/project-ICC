# Uitleg – EasyTech IoT Monitoring

In deze map staan screenshots en bewijsstukken van het IoT-monitoringsysteem.  
Hieronder staat kort uitgelegd wat elk onderdeel laat zien.

---

## architecture.png

Dit diagram laat de opbouw van het systeem zien.

De ESP32 meet temperatuur en vibratie en stuurt deze via HTTP naar Node-RED.  
Node-RED verwerkt de data, slaat alles op in een SQLite database en toont de resultaten in een dashboard.

De ESP32 gebruikt een lokale buffer, automatische reconnect en retries.  
Elk bericht heeft een uniek nummer (`seq`), zodat dezelfde meting niet meerdere keren wordt opgeslagen.

Kort samengevat: ESP32 → Node-RED → Database → Dashboard.

---

## dashboard_screenshot.png

Dit screenshot toont het Node-RED dashboard.

Hierop zijn te zien:
- Temperatuur grafiek
- Vibratie grafiek
- Status (OK / NOT_OK)
- Historische data
- Export naar CSV

Dit laat zien dat de metingen correct binnenkomen en live worden weergegeven.

---

## esp32_serial_output.png

Dit is de Serial Monitor output van de ESP32.

Hierin is zichtbaar:
- De JSON berichten
- Oplopende `seq` nummers
- Queue count
- HTTP OK en HTTP FAIL meldingen

Tijdens testen is te zien dat bij verbindingsproblemen de queue oploopt en bij reconnect automatisch weer leegloopt.

---

## Samenvatting

De ESP32 blijft meten tijdens netwerkproblemen door gebruik te maken van een buffer.  
Zodra de verbinding terug is, worden de opgeslagen metingen automatisch verstuurd.

Door het gebruik van unieke `seq` nummers wordt voorkomen dat dezelfde meting meerdere keren in de database terechtkomt.

Dit project laat zien hoe met een ESP32, Node-RED en SQLite een betrouwbaar IoT monitoringsysteem kan worden gebouwd.

# project-ICC
# EasyTech IoT Monitoring – ESP32 + Node-RED

Dit project maakt data met ESP32 aan de hand van sensoren, dit wordt dan doorverstuurd naar een raspberry met nodered die de data weergeeft in een grafiek

de communicatie tussen de ESP32 en raspberry is HTTP

## Architectuur

ESP32  
→ HTTP POST (JSON)  
→ Node-RED (HTTP In + data transformeren)  
→ SQLite database  
→ Dashboard (temperatuur, vibratie, status, historie, export)

## Functionaliteit

### ESP32
- Leest temperatuur via BME280
- Leest vibratie via piezo + ADC
- Bouwt JSON payload met:
  - node_id
  - seq 
  - temperature
  - vibration
  - status
- Versturen via HTTP POST

### Betrouwbaarheid 
- Lokale queue op de ESP32
- Retry & automatische reconnect wanneer er wifi uitvalt
- retries duren langer bij herhalende falen, (backoff)
- Unieke `seq` per bericht 
- duplicaties worden gedropped op `(node_id, seq)`

### Node-RED
- HTTP endpoint (`/esp/data`)
- JSON parsing
- Normalisatie + timestamp
- Opslag in SQLite (`measurements`)
- Dashboard:
  - Live temperatuur
  - Live vibratie
  - Status (OK / NOT_OK)
  - Historische grafieken
  - CSV export

## Auto reconnect

de verbinding wordt continu bekeken door de ESP32, en zal automatisch proberen te reconnecten bij uitval van verbinding.
Dit werkt zonder reboot of herupload.

## video's
er zijn videos opgenomen voor het bewijs van werking. dit staat onder esp32 en bereikt het volgende:
- Queue buffering
- WiFi disconnect / reconnect
- Server down / up
- duplicaties droppen

## Gebruikte soft-hardware

- ESP32 (Arduino framework)
- BME280 sensor
- Node-RED
- SQLite
- HTTP (JSON)
- Git + GitHub


## mogelijk gemaakt door:

Tymon Schuringa, Ziyad Al Gebri, Muhand Mohamed,  Ayham Ghabra, Nawras Almousa, Hamzah Al Mokdad.
Hanze – ICT Infrastructure & Security  
Project ICC – EasyTech IoT Monitoring
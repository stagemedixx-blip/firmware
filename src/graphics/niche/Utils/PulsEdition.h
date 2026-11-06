#pragma once

/*
PULS-Edition (StageMedixx): Bedienung der Trupp-Melder für die Einsatzleitsoftware PULS

- Menütaste lang: Statusliste (1, 2, 3, 4, 5, 6, 7, 8, Pause, 0 = NOTRUF) mit Bedeutung,
  Auswahl wird sofort an die Einsatzleitung gesendet (kein Empfänger-Menü)
- Alarm von PULS (Nachricht mit Glocke): großer Alarm-Bildschirm, langer Druck = Status 3 (übernommen)
- Funktionstaste 3 Sekunden halten: NOTRUF
- Ziel: erster Favoriten-Knoten (Basisstation der Einsatzleitung), sonst der Hauptkanal
*/

#include "configuration.h"

#if defined(PULS_EDITION) && defined(MESHTASTIC_INCLUDE_NICHE_GRAPHICS)

#include "mesh/MeshTypes.h"
#include <string>

namespace PulsEdition
{
struct StatusEintrag {
    char code;        // gesendet wird diese Ziffer (bzw. „Pause“)
    const char *text; // Bedeutung (UTF-8)
};

constexpr uint8_t ANZAHL_STATUS = 10;
extern const StatusEintrag STATUS[ANZAHL_STATUS];

// Zustand (nur im Speicher, nach Neustart unbekannt)
extern char aktuellerStatus;    // '\0' = unbekannt
extern uint32_t statusZeit;     // Epoch-Sekunden (0 = keine Uhrzeit)
extern bool alarmOffen;         // Alarm empfangen, noch nicht quittiert
extern std::string alarmText;   // Text des letzten Alarms
extern uint32_t alarmZeit;      // Epoch-Sekunden
extern uint32_t alarmMillis;    // millis() beim Eingang

void init(); // Grundeinstellungen beim ersten Start setzen

void ziel(NodeNum &dest, uint8_t &channel);
NodeNum basisstation(); // erster Favorit oder 0

void sende(const char *text, bool notruf = false);
void sendeStatus(uint8_t index); // Index in STATUS
void notruf();
uint8_t vorschlag(); // sinnvoller nächster Status als Index in STATUS
const char *statusText(char code);

void alarmEmpfangen(const std::string &text);

// Paket vom Handy (Trupp-App): Status bzw. Notruf übernehmen
void vomTelefon(const meshtastic_MeshPacket &p);

// Wird nach jeder Änderung aufgerufen (z. B. Anzeige neu zeichnen)
extern void (*geaendert)();
} // namespace PulsEdition

#endif

#pragma once

/*
PULS-Edition (StageMedixx): vereinfachte Bedienung für Trupp-Melder

- Hauptmenü öffnet direkt die Statusliste (vorgefertigte Nachrichten)
- Auswahl sendet sofort an die Einsatzleitung, ohne Empfänger-Menü
  (an den ersten Favoriten-Knoten = Basisstation, sonst an den Hauptkanal)
- Notruf: Funktionstaste 3 Sekunden halten
*/

#include "configuration.h"

#if defined(PULS_EDITION) && defined(MESHTASTIC_INCLUDE_NICHE_GRAPHICS)

#include "mesh/MeshTypes.h"

namespace PulsEdition
{
// Ziel für Status und Notruf: erster Favorit (Basisstation der Einsatzleitung), sonst Hauptkanal
void ziel(NodeNum &dest, uint8_t &channel);
// Text an die Einsatzleitung senden. notruf=true: als Alarm-Paket (ALERT_APP), langer Bestätigungston
void sende(const char *text, bool notruf = false);
} // namespace PulsEdition

#endif

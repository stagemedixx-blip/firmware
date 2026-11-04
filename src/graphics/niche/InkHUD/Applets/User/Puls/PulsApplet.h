#ifdef MESHTASTIC_INCLUDE_INKHUD

/*
PULS-Edition: Hauptanzeige des Trupp-Melders

- Ruhe: aktueller Status groß (Kästchen mit Ziffer + Bedeutung), seit wann, letzter Alarm, Verbindung zur UHS
- Alarm (Nachricht mit Glocke von PULS): großer Alarm-Bildschirm, langer Druck = Status 3 (Einsatz übernommen)
*/

#pragma once

#include "configuration.h"

#include "graphics/niche/InkHUD/Applet.h"
#include "modules/TextMessageModule.h"

namespace NicheGraphics::InkHUD
{

class PulsApplet : public Applet
{
  public:
    void onRender(bool full) override;
    void onActivate() override;
    void onDeactivate() override;
    void onForeground() override;
    void onButtonShortPress() override;
    void onButtonLongPress() override;
    bool approveNotification(Notification &n) override;

    int onReceiveTextMessage(const meshtastic_MeshPacket *p);
    void zustandGeaendert(); // aus PulsEdition

  protected:
    void zeichneAlarm();
    void zeichneRuhe();
    void eingabenSetzen();

    CallbackObserver<PulsApplet, const meshtastic_MeshPacket *> textMessageObserver =
        CallbackObserver<PulsApplet, const meshtastic_MeshPacket *>(this, &PulsApplet::onReceiveTextMessage);
};

} // namespace NicheGraphics::InkHUD

#endif

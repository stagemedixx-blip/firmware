#ifdef MESHTASTIC_INCLUDE_INKHUD

#include "./PulsApplet.h"

#if defined(PULS_EDITION)

#include "RTC.h"
#include "graphics/niche/Utils/PulsEdition.h"
#include "mesh/NodeDB.h"
#include <cstring>

using namespace NicheGraphics;

static InkHUD::PulsApplet *pulsInstanz = nullptr;
static void pulsGeaendert()
{
    if (pulsInstanz)
        pulsInstanz->zustandGeaendert();
}

void InkHUD::PulsApplet::onActivate()
{
    pulsInstanz = this;
    PulsEdition::geaendert = pulsGeaendert;
    textMessageObserver.observe(textMessageModule);
    eingabenSetzen();
}

void InkHUD::PulsApplet::onDeactivate()
{
    textMessageObserver.unobserve(textMessageModule);
    if (pulsInstanz == this) {
        pulsInstanz = nullptr;
        PulsEdition::geaendert = nullptr;
    }
}

void InkHUD::PulsApplet::onForeground()
{
    eingabenSetzen();
}

// Bei offenem Alarm übernimmt die Anzeige beide Tastendrücke selbst (langer Druck = Status 3).
// Sonst öffnet ein langer Druck wie gewohnt die Statusliste.
void InkHUD::PulsApplet::eingabenSetzen()
{
    setInputsSubscribed(BUTTON_SHORT | BUTTON_LONG, PulsEdition::alarmOffen);
}

void InkHUD::PulsApplet::zustandGeaendert()
{
    eingabenSetzen();
    requestUpdate(Drivers::EInk::UpdateTypes::FULL);
}

int InkHUD::PulsApplet::onReceiveTextMessage(const meshtastic_MeshPacket *p)
{
    if (!isActive())
        return 0;
    if (getFrom(p) == nodeDB->getNodeNum())
        return 0;
    // Nur Direktnachrichten an diesen Melder (Alarme und Nachrichten von PULS)
    if (p->to != nodeDB->getNodeNum())
        return 0;

    std::string text((const char *)p->decoded.payload.bytes, p->decoded.payload.size);
    bool glocke = false;
    std::string sauber;
    for (char c : text) {
        if (c == 7)
            glocke = true;
        else
            sauber += c;
    }
    if (glocke || sauber.rfind("ALARM", 0) == 0 || sauber.rfind("ÜBUNG", 0) == 0 || sauber.rfind("PULS", 0) == 0) {
        PulsEdition::alarmEmpfangen(sauber); // ruft zustandGeaendert()
        requestAutoshow();
    } else {
        // Sonstige Nachricht der Einsatzleitung: als letzter Hinweis anzeigen, nicht als Alarm
        PulsEdition::alarmText = sauber;
        PulsEdition::alarmZeit = getTime();
        requestAutoshow();
        requestUpdate();
    }
    return 0;
}

void InkHUD::PulsApplet::onButtonShortPress()
{
    // Bei offenem Alarm: kurzer Druck stellt nur den Ton ab (erledigt InkHUD bereits), Anzeige bleibt
    requestUpdate(Drivers::EInk::UpdateTypes::FAST);
}

void InkHUD::PulsApplet::onButtonLongPress()
{
    if (PulsEdition::alarmOffen) {
        // Alarm übernehmen = Status 3
        for (uint8_t i = 0; i < PulsEdition::ANZAHL_STATUS; i++) {
            if (PulsEdition::STATUS[i].code == '3') {
                PulsEdition::sendeStatus(i);
                break;
            }
        }
        return;
    }
    inkhud->openMenu();
}

bool InkHUD::PulsApplet::approveNotification(Notification &n)
{
    // Nachrichten zeigt diese Anzeige selbst
    if (n.type == Notification::Type::NOTIFICATION_MESSAGE_DIRECT || n.type == Notification::Type::NOTIFICATION_MESSAGE_BROADCAST)
        return false;
    return true;
}

void InkHUD::PulsApplet::onRender(bool full)
{
    if (PulsEdition::alarmOffen)
        zeichneAlarm();
    else
        zeichneRuhe();
}

// Großer Alarm-Bildschirm
void InkHUD::PulsApplet::zeichneAlarm()
{
    // Schwarzer Balken oben: ALARM + Uhrzeit
    const int16_t balkenH = fontLarge.lineHeight() + 10;
    fillRect(0, 0, width(), balkenH, BLACK);
    std::string kopf = "ALARM";
    std::string zeit = getTimeString(PulsEdition::alarmZeit);
    if (!zeit.empty())
        kopf += "  " + zeit;
    setFont(fontLarge);
    setTextColor(WHITE);
    printThick(X(0.5), balkenH / 2, parse(kopf), 2, 1);
    setTextColor(BLACK);

    // Schwarzer Balken unten: Bedienhinweis
    setFont(fontSmall);
    const int16_t fussH = fontSmall.lineHeight() + 6;
    fillRect(0, height() - fussH, width(), fussH, BLACK);
    setTextColor(WHITE);
    printAt(X(0.5), height() - fussH / 2, parse("Lang drücken = Status 3"), CENTER, MIDDLE);
    setTextColor(BLACK);

    // Alarmtext so groß wie möglich
    std::string text = PulsEdition::alarmText;
    for (const char *weg : {"ALARM ", "ALARM"}) {
        if (text.rfind(weg, 0) == 0) {
            text.erase(0, strlen(weg));
            break;
        }
    }
    // Teile („|“) als eigene Zeilen
    std::string mitUmbruch;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '|') {
            while (!mitUmbruch.empty() && mitUmbruch.back() == ' ')
                mitUmbruch.pop_back();
            mitUmbruch += '\n';
            while (i + 1 < text.size() && text[i + 1] == ' ')
                i++;
        } else
            mitUmbruch += text[i];
    }
    text = parse(mitUmbruch);
    const int16_t oben = balkenH + 4;
    const uint16_t verfuegbar = height() - balkenH - fussH - 8;
    setFont(fontLarge);
    if (getWrappedTextHeight(2, width() - 4, text) > verfuegbar)
        setFont(fontMedium);
    if (getWrappedTextHeight(2, width() - 4, text) > verfuegbar)
        setFont(fontSmall);
    printWrapped(2, oben, width() - 4, text);
}

// Ruhe-Anzeige: Status groß
void InkHUD::PulsApplet::zeichneRuhe()
{
    // Kopfzeile: StageMedixx + eigener Name
    meshtastic_NodeInfoLite *ich = nodeDB->getMeshNode(nodeDB->getNodeNum());
    std::string kopf = "StageMedixx";
    if (ich && ich->has_user) {
        kopf += " · ";
        kopf += ich->user.long_name;
    }
    setFont(fontSmall);
    drawHeader(parse(kopf));
    const int16_t oben = getHeaderHeight() + 4;

    // Status-Kästchen mit Ziffer
    const char code = PulsEdition::aktuellerStatus;
    const int16_t box = Y(0.30);
    const int16_t boxX = 4;
    fillRect(boxX, oben, box, box, BLACK);
    setFont(AppletFont(FreeSans24pt_Win1253, AppletFont::WINDOWS_1253, -5, 3)); // große Ziffer (nur ASCII)
    setTextColor(WHITE);
    std::string ziffer = code ? std::string(1, code) : std::string("-");
    printThick(boxX + box / 2, oben + box / 2, ziffer, 2, 2);
    setTextColor(BLACK);

    // Bedeutung + seit wann
    const int16_t textX = boxX + box + 6;
    setFont(fontSmall);
    printAt(textX, oben, "Status", LEFT, TOP);
    setFont(fontMedium);
    std::string bedeutung = code ? parse(PulsEdition::statusText(code)) : std::string("unbekannt");
    printWrapped(textX, oben + fontSmall.lineHeight() + 2, width() - textX - 2, bedeutung);
    if (code && PulsEdition::statusZeit) {
        setFont(fontSmall);
        std::string seit = "seit " + getTimeString(PulsEdition::statusZeit);
        printAt(textX, oben + box, seit, LEFT, BOTTOM);
    }

    // Letzter Alarm / letzte Nachricht der Einsatzleitung
    int16_t y = oben + box + 8;
    for (int16_t x = 0; x < width(); x += 2)
        drawPixel(x, y - 4, BLACK);
    if (!PulsEdition::alarmText.empty()) {
        setFont(fontSmall);
        std::string zeit = getTimeString(PulsEdition::alarmZeit);
        printAt(2, y, parse(std::string("Letzte Meldung") + (zeit.empty() ? "" : " " + zeit) + ":"), LEFT, TOP);
        y += fontSmall.lineHeight() + 2;
        std::string t = PulsEdition::alarmText;
        for (char &c : t)
            if (c == '|')
                c = ',';
        setFont(fontMedium);
        setCrop(0, y, width(), height() - y - fontSmall.lineHeight() - 6);
        printWrapped(2, y, width() - 4, parse(t));
        resetCrop();
    } else {
        setFont(fontSmall);
        printAt(2, y, parse("Lang drücken = Status senden"), LEFT, TOP);
        printAt(2, y + fontSmall.lineHeight() + 2, parse("Lichttaste 3 s halten = NOTRUF"), LEFT, TOP);
    }

    // Fußzeile: Verbindung zur Einsatzleitung (Basisstation)
    setFont(fontSmall);
    std::string fuss;
    const NodeNum b = PulsEdition::basisstation();
    if (!b) {
        fuss = "UHS nicht als Favorit gesetzt";
    } else {
        meshtastic_NodeInfoLite *node = nodeDB->getMeshNode(b);
        const uint32_t jetzt = getTime();
        if (node && node->last_heard && jetzt >= node->last_heard) {
            const uint32_t min = (jetzt - node->last_heard) / 60;
            fuss = "UHS gehört vor " + std::to_string(min) + " min";
        } else
            fuss = "UHS noch nicht gehört";
    }
    for (int16_t x = 0; x < width(); x += 2)
        drawPixel(x, height() - fontSmall.lineHeight() - 4, BLACK);
    printAt(2, height() - 1, parse(fuss), LEFT, BOTTOM);
}

#endif // PULS_EDITION
#endif // MESHTASTIC_INCLUDE_INKHUD

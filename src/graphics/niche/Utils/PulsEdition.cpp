#include "./PulsEdition.h"

#if defined(PULS_EDITION) && defined(MESHTASTIC_INCLUDE_NICHE_GRAPHICS)

#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "Router.h"
#include "buzz.h"
#include "mesh/SinglePortModule.h"
#include "modules/PositionModule.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace PulsEdition
{
const StatusEintrag STATUS[ANZAHL_STATUS] = {
    {'1', "Einsatzbereit (Funk)"},
    {'2', "Einsatzbereit (Standort)"},
    {'3', "Einsatz übernommen"},
    {'4', "Am Einsatzort"},
    {'7', "Patient aufgenommen"},
    {'8', "Am Transportziel"},
    {'5', "Sprechwunsch"},
    {'6', "Nicht einsatzbereit"},
    {'P', "Pause"},
    {'0', "NOTRUF"},
};

char aktuellerStatus = '\0';
uint32_t statusZeit = 0;
bool alarmOffen = false;
std::string alarmText;
uint32_t alarmZeit = 0;
uint32_t alarmMillis = 0;
void (*geaendert)() = nullptr;


// Befehle von PULS über Port 256 (PRIVATE_APP), nur von der Basisstation (Favorit) bzw. ohne Favorit vom eigenen Kanal:
//   "GPS <Sek.>"  Positions-Takt einstellen (30–1800 s), Antwort "GPS <Sek.>"
//   "POS"         aktuelle Position sofort schicken
class PulsModul : public SinglePortModule
{
  public:
    PulsModul() : SinglePortModule("puls", meshtastic_PortNum_PRIVATE_APP) {}

  protected:
    ProcessMessage handleReceived(const meshtastic_MeshPacket &mp) override
    {
        if (mp.from == nodeDB->getNodeNum() || mp.to != nodeDB->getNodeNum())
            return ProcessMessage::CONTINUE;
        const NodeNum b = basisstation();
        if (b && mp.from != b)
            return ProcessMessage::CONTINUE;

        char t[33] = {0};
        size_t n = mp.decoded.payload.size < 32 ? mp.decoded.payload.size : 32;
        memcpy(t, mp.decoded.payload.bytes, n);

        if (strncmp(t, "GPS ", 4) == 0) {
            long sek = strtol(t + 4, nullptr, 10);
            if (sek < 30)
                sek = 30;
            if (sek > 1800)
                sek = 1800;
            const uint32_t gps = sek < 60 ? (uint32_t)sek : 60; // GPS misst mindestens jede Minute frisch
            if (config.position.position_broadcast_secs != (uint32_t)sek || config.position.gps_update_interval != gps) {
                config.position.position_broadcast_secs = sek;
                config.position.gps_update_interval = gps;
                nodeDB->saveToDisk(SEGMENT_CONFIG);
                LOG_INFO("PULS: GPS-Takt %ld s", sek);
            }
            meshtastic_MeshPacket *r = allocDataPacket();
            if (r) {
                r->to = mp.from;
                r->channel = mp.channel;
                r->want_ack = false;
                int len = snprintf((char *)r->decoded.payload.bytes, sizeof(r->decoded.payload.bytes), "GPS %ld", sek);
                r->decoded.payload.size = len > 0 ? len : 0;
                service->sendToMesh(r, RX_SRC_LOCAL, false);
            }
            return ProcessMessage::STOP;
        }
        if (strncmp(t, "POS", 3) == 0) {
            if (positionModule)
                positionModule->sendOurPosition(mp.from, false, mp.channel);
            return ProcessMessage::STOP;
        }
        return ProcessMessage::CONTINUE;
    }
};
static PulsModul *pulsModul = nullptr;

static void melden()
{
    if (geaendert)
        geaendert();
}

void init()
{
    if (!pulsModul)
        pulsModul = new PulsModul();

    // Nur auf frischen Geräten: lauter Dauer-Alarm bei Nachrichten mit Glocke, Displaylicht flackert
    if (!moduleConfig.external_notification.enabled) {
        moduleConfig.has_external_notification = true;
        auto &en = moduleConfig.external_notification;
        en.enabled = true;
        en.alert_bell = true;
        en.alert_bell_buzzer = true;
        en.use_pwm = true;
        en.active = true;
        en.output = 32 + 11; // Frontlicht (PIN_EINK_EN) flackert
        en.output_ms = 300;
        en.nag_timeout = 60;
        nodeDB->saveToDisk(SEGMENT_MODULECONFIG);
    }
}

NodeNum basisstation()
{
    const NodeNum ich = nodeDB->getNodeNum();
    const uint32_t n = nodeDB->getNumMeshNodes();
    for (uint32_t i = 0; i < n; i++) {
        const meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
        if (node && node->num != ich && node->is_favorite)
            return node->num;
    }
    return 0;
}

void ziel(NodeNum &dest, uint8_t &channel)
{
    const NodeNum b = basisstation();
    if (b) {
        dest = b;
        channel = nodeDB->getMeshNodeChannel(b);
    } else {
        dest = NODENUM_BROADCAST;
        channel = 0;
    }
}

void sende(const char *text, bool istNotruf)
{
    meshtastic_MeshPacket *p = router->allocForSending();
    if (!p)
        return;
    NodeNum dest;
    uint8_t channel;
    ziel(dest, channel);
    p->decoded.portnum = istNotruf ? meshtastic_PortNum_ALERT_APP : meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->to = dest;
    p->channel = channel;
    p->want_ack = true;
    size_t len = strlen(text);
    if (len > sizeof(p->decoded.payload.bytes))
        len = sizeof(p->decoded.payload.bytes);
    memcpy(p->decoded.payload.bytes, text, len);
    p->decoded.payload.size = len;
    LOG_INFO("PULS: sende %s an %x: %s", istNotruf ? "NOTRUF" : "Status", dest, text);
    service->sendToMesh(p, RX_SRC_LOCAL, true);
    if (istNotruf)
        playLongBeep();
    else
        playBeep();
}

void sendeStatus(uint8_t index)
{
    if (index >= ANZAHL_STATUS)
        return;
    const char code = STATUS[index].code;
    if (code == '0') {
        notruf();
        return;
    }
    if (code == 'P') {
        sende("Pause");
    } else {
        char t[2] = {code, '\0'};
        sende(t);
    }
    if (code != '5') { // Sprechwunsch ist kein Fahrzeugstatus
        aktuellerStatus = code;
        statusZeit = getTime();
    }
    alarmOffen = false; // jede Rückmeldung quittiert den Alarm
    melden();
}

void notruf()
{
    sende("0", true);
    alarmOffen = false;
    melden();
}

uint8_t vorschlag()
{
    char naechster = '3';
    if (alarmOffen)
        naechster = '3';
    else
        switch (aktuellerStatus) {
        case '3':
            naechster = '4';
            break;
        case '4':
            naechster = '7';
            break;
        case '7':
            naechster = '8';
            break;
        case '8':
            naechster = '1';
            break;
        case '1':
        case '2':
        case 'P':
        case '6':
        default:
            naechster = aktuellerStatus ? '2' : '3';
            if (aktuellerStatus == '2')
                naechster = '1';
            break;
        }
    for (uint8_t i = 0; i < ANZAHL_STATUS; i++)
        if (STATUS[i].code == naechster)
            return i;
    return 0;
}

const char *statusText(char code)
{
    for (uint8_t i = 0; i < ANZAHL_STATUS; i++)
        if (STATUS[i].code == code)
            return STATUS[i].text;
    return "";
}

void vomTelefon(const meshtastic_MeshPacket &p)
{
    if (p.which_payload_variant != meshtastic_MeshPacket_decoded_tag)
        return;
    const auto &d = p.decoded;
    std::string t((const char *)d.payload.bytes, d.payload.size);
    if (d.portnum == meshtastic_PortNum_ALERT_APP) {
        alarmOffen = false;
        melden();
        return;
    }
    if (d.portnum != meshtastic_PortNum_TEXT_MESSAGE_APP)
        return;
    char code = 0;
    if (t == "Pause")
        code = 'P';
    else if (t.size() == 1 && t[0] >= '0' && t[0] <= '9')
        code = t[0];
    if (!code)
        return;
    if (code != '5' && code != '0') {
        aktuellerStatus = code;
        statusZeit = getTime();
    }
    alarmOffen = false;
    melden();
}

void alarmEmpfangen(const std::string &text)
{
    alarmText = text;
    alarmZeit = getTime();
    alarmMillis = millis();
    alarmOffen = true;
    melden();
}
} // namespace PulsEdition

#endif

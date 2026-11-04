#include "./PulsEdition.h"

#if defined(PULS_EDITION) && defined(MESHTASTIC_INCLUDE_NICHE_GRAPHICS)

#include "MeshService.h"
#include "NodeDB.h"
#include "RTC.h"
#include "Router.h"
#include "buzz.h"
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

static void melden()
{
    if (geaendert)
        geaendert();
}

void init()
{
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

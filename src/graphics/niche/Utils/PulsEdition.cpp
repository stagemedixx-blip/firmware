#include "./PulsEdition.h"

#if defined(PULS_EDITION) && defined(MESHTASTIC_INCLUDE_NICHE_GRAPHICS)

#include "MeshService.h"
#include "NodeDB.h"
#include "Router.h"
#include "buzz.h"
#include <cstring>

void PulsEdition::ziel(NodeNum &dest, uint8_t &channel)
{
    dest = NODENUM_BROADCAST;
    channel = 0;
    const NodeNum ich = nodeDB->getNodeNum();
    const uint32_t n = nodeDB->getNumMeshNodes();
    for (uint32_t i = 0; i < n; i++) {
        const meshtastic_NodeInfoLite *node = nodeDB->getMeshNodeByIndex(i);
        if (node && node->num != ich && node->is_favorite) {
            dest = node->num;
            channel = nodeDB->getMeshNodeChannel(node->num);
            return;
        }
    }
}

void PulsEdition::sende(const char *text, bool notruf)
{
    meshtastic_MeshPacket *p = router->allocForSending();
    if (!p)
        return;
    NodeNum dest;
    uint8_t channel;
    ziel(dest, channel);
    p->decoded.portnum = notruf ? meshtastic_PortNum_ALERT_APP : meshtastic_PortNum_TEXT_MESSAGE_APP;
    p->to = dest;
    p->channel = channel;
    p->want_ack = true;
    size_t len = strlen(text);
    if (len > sizeof(p->decoded.payload.bytes))
        len = sizeof(p->decoded.payload.bytes);
    memcpy(p->decoded.payload.bytes, text, len);
    p->decoded.payload.size = len;
    LOG_INFO("PULS: sende %s an %x: %s", notruf ? "NOTRUF" : "Status", dest, text);
    service->sendToMesh(p, RX_SRC_LOCAL, true); // an das Netz, Kopie an die App
    if (notruf)
        playLongBeep();
    else
        playBeep();
}

#endif

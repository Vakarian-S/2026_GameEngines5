#include "net/NetworkLibrary.h"

#include <stdexcept>
#include <string>
#include <algorithm>
#include <iterator>

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

namespace
{
    /**
     * The event list GNS's status callback writes to. The callback is a plain function with no
     * context pointer, and GNS is one per program, so this one pointer is enough
     */
    std::vector<NET::ConnectionEvent>* statusEvents = nullptr;

    /** Turns a GNS connection state into the engine's own */
    NET::ConnectionState ToConnectionState(const ESteamNetworkingConnectionState state)
    {
        switch (state)
        {
        case k_ESteamNetworkingConnectionState_None: return NET::ConnectionState::None;
        case k_ESteamNetworkingConnectionState_Connecting: return NET::ConnectionState::Connecting;
        case k_ESteamNetworkingConnectionState_Connected: return NET::ConnectionState::Connected;
        case k_ESteamNetworkingConnectionState_ClosedByPeer: return NET::ConnectionState::ClosedByPeer;
        case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: return
                NET::ConnectionState::ProblemDetectedLocally;
        case k_ESteamNetworkingConnectionState_FindingRoute:
        case k_ESteamNetworkingConnectionState_FinWait:
        case k_ESteamNetworkingConnectionState_Linger:
        case k_ESteamNetworkingConnectionState_Dead:
        case k_ESteamNetworkingConnectionState__Force32Bit:
            return NET::ConnectionState::Other;
        }
        return NET::ConnectionState::Other;
    }

    /** GNS calls this from inside RunCallbacks, on the thread that called it, once per change */
    void OnStatusChanged(SteamNetConnectionStatusChangedCallback_t* change)
    {
        NET::ConnectionEvent event{};
        event.connection = change->m_hConn;
        event.listenSocket = change->m_info.m_hListenSocket;
        event.oldState = ToConnectionState(change->m_eOldState);
        event.state = ToConnectionState(change->m_info.m_eState);
        event.endReason = change->m_info.m_eEndReason;
        std::ranges::copy(change->m_info.m_szEndDebug,
                          event.endMessage.begin());
        statusEvents->push_back(event);
    }
}

NET::NetworkLibrary::NetworkLibrary()
{
    SteamNetworkingErrMsg error{};
    if (!GameNetworkingSockets_Init(nullptr, error))
    {
        throw std::runtime_error(std::string("GameNetworkingSockets failed to start: ") + error);
    }
    statusEvents = &events;
    SteamNetworkingUtils()->SetGlobalCallback_SteamNetConnectionStatusChanged(&OnStatusChanged);
}

NET::NetworkLibrary::~NetworkLibrary()
{
    GameNetworkingSockets_Kill();
    statusEvents = nullptr;
}

std::span<const NET::ConnectionEvent> NET::NetworkLibrary::PollConnectionEvents()
{
    events.clear();
    SteamNetworkingSockets()->RunCallbacks();
    return events;
}

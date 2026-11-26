/*
 * Joining and leaving over GNS, with a server World and two client Worlds in one program.
 *
 * x64 only, because it links GNS. Each World gets every connection event, as the applications
 * hand them over, so each World's systems must pick out their own. Each World's prints go to
 * its own log, so the checks can read what each side said.
 *
 * It listens on port 27098, so a game server on the default port 27020 does not get in the way.
 *
 * One program, one test: it counts its own checks and returns non zero if any of them fail.
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ecs/Phase.h"
#include "ecs/Query.h"
#include "ecs/SystemRegistry.h"
#include "ecs/World.h"
#include "game/systems/AcceptClientsSystem.h"
#include "game/systems/ClientReceiveSystem.h"
#include "game/systems/ServerReceiveSystem.h"
#include "net/ClientConnection.h"
#include "net/ClientTable.h"
#include "net/Components.h"
#include "net/ConnectionEvents.h"
#include "net/NetworkLibrary.h"
#include "net/ServerSocket.h"

namespace
{
    int checks = 0;
    int failures = 0;

    constexpr const char* Pass = "\x1b[1;32m";
    constexpr const char* Fail = "\x1b[1;91m";
    constexpr const char* Reset = "\x1b[0m";

    /** The port the test server listens on */
    constexpr std::uint16_t kPort = 27098;
    /** Most frames a step waits for its answer. 300 frames of 10 ms is 3 s */
    constexpr int kMaxFrames = 300;

    /** Prints one named expectation, and remembers whether it held. */
    void Check(const char* what, const bool held)
    {
        ++checks;
        if (!held) ++failures;
        std::cout << (held ? "  pass  " : "  FAIL  ") << what << "\n";
    }

    /** One program's World and systems, as its scene holds them, and everything its systems printed */
    struct Side
    {
        World world{WorldConfig{64, 256 * 1024}};
        SystemRegistry systems{world};
        /** What this side's systems printed */
        std::ostringstream log;
        /** The phase this side's network systems run in: Fixed on the server, Update on a client */
        Phase phase;

        explicit Side(const Phase phase_) : phase(phase_)
        {
            world.GetResources().Insert<NET::ConnectionEventQueue>();
        }

        /** Runs this side's phase once, with std::cout sent to log */
        void Run()
        {
            std::streambuf* console = std::cout.rdbuf(log.rdbuf());
            try
            {
                systems.RunPhase(phase, 1.0f / 60.0f);
            }
            catch (...)
            {
                std::cout.rdbuf(console);
                throw;
            }
            std::cout.rdbuf(console);
        }

        /** @return how many times text appears in this side's log */
        [[nodiscard]] std::size_t Count(const std::string& text) const
        {
            const std::string all = log.str();
            std::size_t count = 0;
            for (std::size_t at = all.find(text); at != std::string::npos; at = all.find(text, at + text.size())) ++count;
            return count;
        }
    };

    /** @return a server side, built as BossFightScene builds one */
    std::unique_ptr<Side> MakeServer()
    {
        auto server = std::make_unique<Side>(Phase::Fixed);
        server->world.GetResources().Insert<NET::ServerSocket>(kPort);
        server->world.GetResources().Insert<NET::ClientTable>();
        server->systems.Add(Phase::Fixed, std::make_unique<AcceptClientsSystem>(server->world));
        server->systems.Add(Phase::Fixed, std::make_unique<ServerReceiveSystem>(server->world));
        return server;
    }

    /** @return a client side connecting to the test server, built as BossFightScene builds one */
    std::unique_ptr<Side> MakeClient()
    {
        auto client = std::make_unique<Side>(Phase::Update);
        client->world.GetResources().Insert<NET::ClientConnection>("127.0.0.1:" + std::to_string(kPort));
        client->systems.Add(Phase::Update, std::make_unique<ClientReceiveSystem>(client->world));
        return client;
    }

    /**
     * Polls GNS, hands every event to every side, and runs each side, once per frame
     * @return true once done() returns true, false after kMaxFrames frames
     */
    template <typename Done>
    bool Pump(NET::NetworkLibrary& library, const std::vector<Side*>& sides, Done done)
    {
        for (int frame = 0; frame < kMaxFrames; ++frame)
        {
            const std::span<const NET::ConnectionEvent> events = library.PollConnectionEvents();
            for (Side* side : sides)
            {
                auto& queue = side->world.GetResources().Get<NET::ConnectionEventQueue>();
                queue.events.insert(queue.events.end(), events.begin(), events.end());
            }
            for (Side* side : sides) side->Run();
            if (done()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }

    /** @return how many entities in world carry a NetworkIdComponent */
    std::size_t CountPlayers(World& world)
    {
        std::size_t count = 0;
        Query players(world, COMPONENTS::MaskOf<NET::NetworkIdComponent>());
        players.ForEachChunk([&count](ChunkView chunk) { count += chunk.Count(); });
        return count;
    }
}

int main()
{
    std::cout << "NetTests\n\n";
    try
    {
        /* First, so it is destroyed last: no socket or connection outlives GNS */
        NET::NetworkLibrary library;
        const std::unique_ptr<Side> server = MakeServer();
        const NET::ClientTable& table = server->world.GetResources().Get<NET::ClientTable>();

        std::cout << "TEST CASE: client A joins\n";
        std::unique_ptr<Side> clientA = MakeClient();
        const bool aWelcomed = Pump(library, {server.get(), clientA.get()}, [&]
        {
            return clientA->Count("[Client] welcome") == 1;
        });
        Check("client A gets a Welcome", aWelcomed);
        Check("the Welcome names client 1 and player NetworkId 1",
              clientA->Count("welcome: client 1, player NetworkId 1") == 1);
        Check("the server holds one player", table.PlayerCount() == 1 && CountPlayers(server->world) == 1);

        std::cout << "\nTEST CASE: client B joins, and A hears about it\n";
        std::unique_ptr<Side> clientB = MakeClient();
        const bool bWelcomed = Pump(library, {server.get(), clientA.get(), clientB.get()}, [&]
        {
            return clientB->Count("[Client] welcome") == 1 && clientA->Count("player 2 joined") == 1;
        });
        Check("client B gets a Welcome as client 2",
              bWelcomed && clientB->Count("welcome: client 2, player NetworkId 2") == 1);
        Check("client A hears that player 2 joined", clientA->Count("[Client] player 2 joined") == 1);
        Check("client B hears nothing about itself", clientB->Count("joined") == 0);
        Check("the server holds two players", table.PlayerCount() == 2 && CountPlayers(server->world) == 2);
        Check("each client sent one Hello, though every World saw every event",
              clientA->Count("sending Hello") == 1 && clientB->Count("sending Hello") == 1);

        std::cout << "\nTEST CASE: client A leaves\n";
        clientA.reset();
        const bool aGone = Pump(library, {server.get(), clientB.get()}, [&]
        {
            return clientB->Count("player 1 left") == 1;
        });
        Check("client B hears that player 1 left", aGone);
        Check("the server drops to one player", table.PlayerCount() == 1 && CountPlayers(server->world) == 1);
        Check("the server logged client 1 leaving", server->Count("client 1 left") == 1);

        std::cout << "\nTEST CASE: client B leaves\n";
        clientB.reset();
        const bool bGone = Pump(library, {server.get()}, [&] { return table.PlayerCount() == 0; });
        Check("the server drops to zero players", bGone && CountPlayers(server->world) == 0);
        Check("the server logged client 2 leaving", server->Count("client 2 left") == 1);
    }
    catch (const std::exception& error)
    {
        ++checks;
        ++failures;
        std::cout << "  FAIL  threw: " << error.what() << "\n";
    }

    std::cout << "\n" << (failures == 0 ? Pass : Fail) << checks - failures << " of " << checks
        << " checks passed" << Reset << "\n";
    return failures == 0 ? 0 : 1;
}

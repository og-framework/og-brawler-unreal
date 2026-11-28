<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerSimNetLanesUImpl.cpp` — rationale

`Source/OGBrawlerUnreal/BrawlerSimNetLanesUImpl.cpp` defines the brawler's og-net sim-lanes factory,
`makeBrawlerSimNetLanes(UOGNetDriver&, bool bIsServer)` (og-input-latency task P5-7). It holds no prohibition, so it has no
guards file.

**If this file and the source disagree, the source is authoritative and this file is stale.**

## §1 What it is

The og-net driver (`UOGNetDriver`, OGNetUnreal) cannot name a game, so a game hands it a factory: a plain function of type
`FOGNetSimLanesFactory` (`OGNetSimLanes.h`). The driver calls it once in each `InitBase`, before it starts its endpoint, with
its role (`bIsServer` is `!bInitAsClient`), and owns what it returns (`OGNetSimLanes-rationale.md`). The brawler's factory returns
`UESimNetLanes<BrawlerSessionTraitsUImpl>` for that role (OGSimulationUnreal's `UESimNetLanes-rationale.md`): the sim transport
over the driver's endpoint and, on a client, og-sim-net's client session and the roster bindings. The world's
`UESimulationSession` finds these lanes through the driver (`GetSimLanes()`) and binds to them (`UESimulationSession-rationale.md`
§16), which is why the session can `static_cast` the driver's `IOGNetSimLanes*` to its own lanes type: this function is the only
factory the game registers.

A `static_assert` in the function checks that its type is the factory type, so a signature change fails where it is typed rather
than at the registration call.

## §2 Where it is registered

The game module's `StartupModule` registers it, after the network-version hook (`OGBrawlerUnreal-rationale.md`). The module
declares the function itself; there is no header, because nothing else names it.

## §3 Why its own file

The lanes' template pulls in the session, og-sim-net's client and server and the driver's headers. The module's `.cpp` stays
small and keeps its own optimization pair; this file instantiates the lanes for the brawler's traits in one translation unit.

# LAN multiplayer

The Unreal client uses a listen-server model. The host process owns the only rules-engine
`GameController`; clients submit authenticated RPC commands and consume replicated views.
There is no dedicated server.

## Flow

1. Enter your name and host a LAN lobby, refresh advertised lobbies, or enter `IP:7777`.
2. The lobby supports two to four players. Every player, including the host, selects Ready.
3. Only the host sees an enabled Start button, and only after every connected player is ready.
4. After Start, the host validates each command against the sending connection and current turn.

`OnlineSubsystemNull` provides LAN advertisement/discovery. Manual IP join remains available
when broadcast discovery is blocked by a firewall or Wi-Fi client isolation.

## Security boundary

`ACatanGameState` replicates the board, turn, scores and public card counts. Exact resources and
playable development-card types are stored in `ACatanPlayerState::PrivateView` and replicated with
`COND_OwnerOnly`. A client never supplies a player name for an action: the server derives identity
from the RPC's owning `PlayerController`.

This protects players from other network clients. A listen host controls the server process and
therefore cannot be cryptographically prevented from inspecting server memory; that requires an
independent trusted server, which is intentionally outside this LAN architecture.

## Multi-process smoke test

Build `CatanEditor`, then run:

```sh
unreal/Catan/Scripts/run_lan_smoke.sh 3
```

The script starts one visible listen host and two clients, marks all players ready, asks the host
to start at the expected population, and passes when the authoritative match-start log appears.
Set `UE_EDITOR` when UnrealEditor is installed elsewhere. Runtime logs include `LAN discovery`,
`Authenticated command`, and `CATAN_SMOKE` markers.

## Automated multiplayer E2E

The reproducible headless scenario starts a host and two clients, readies the
lobby, completes setup and normal turns, then disconnects and reconnects one
client and verifies that its public and private state is restored:

```bash
unreal/Catan/Scripts/run_multiplayer_e2e.sh
```

Set `UE_EDITOR` to override the UnrealEditor executable or
`CATAN_E2E_ADDRESS` to use a non-default listen address. Separate process logs
are retained in the temporary directory printed by the script.

To exercise the same path as the main menu (host session creation, UDP discovery join, and a
separate manual-address join), run:

```sh
unreal/Catan/Scripts/run_lan_lobby_e2e.sh
```

Discovery uses a small Catan UDP query on port `15001`, sent both to loopback and the selected
private-network broadcast. This avoids `OnlineSubsystemNull` sharing its fixed beacon port between
multiple processes on one machine. The actual game connection still uses Unreal networking on
UDP `7777`.

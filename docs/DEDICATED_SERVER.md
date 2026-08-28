# Dedicated server

The dedicated server is a portable, headless C++ process. It owns one
`GameController` per lobby and runs on macOS and Linux; Unreal Engine is not
required on the server machine.

Build and run:

```sh
cmake -S . -B build -DCATAN_BUILD_CONSOLE=OFF
cmake --build build --target catan-dedicated-server
./build/catan-dedicated-server --bind 0.0.0.0 --port 17777 \
  --state-file ./data/catan-dedicated.state
```

Allow inbound TCP `17777` in the host firewall. In the game's **Online** menu,
enter the server IP and port. Choose **Create game on server** to receive a
shareable lobby token and a private player token, or paste a lobby token and
choose **Join game by lobby token**. The local LAN host mode remains available.

Lobby tokens identify rooms and may be shared. Player tokens authenticate one
player and must remain private. Snapshots contain exact resources and
development-card details only for the authenticated player; opponents expose
only public totals. The server can own many independent lobbies (128 by
default, configurable with `--max-lobbies`).

The current transport is plain TCP and is intended for a trusted local network;
tokens prevent one client from reading another player's private state, but they
do not protect against packet capture. Internet deployment should put the
service behind an encrypted tunnel until TLS transport is added.

## Persistent multi-lobby state

Persistence is enabled by default. The server atomically saves all waiting and
active lobbies to `catan-dedicated.state` in its current working directory after
every successful mutation and during a clean shutdown. Use `--state-file PATH`
to put it in a dedicated data directory, or `--no-persistence` only for an
explicitly disposable server. On restart the same lobby tokens, private player
tokens, ready flags and in-progress `GameController` states are restored, so
existing clients can continue using their credentials.

The UE client stores only its own address/lobby/player credential in the
platform user-settings area and exposes **Reconnect as _name_** on the
Dedicated Server page. `RESUME` authenticates that existing identity without
adding a duplicate player and works for both waiting and active games. An
explicit successful lobby leave or an invalid saved credential removes the
local reconnect record; a temporary outage keeps it.

The state file contains every player's private authentication token. It is
created with owner-only permissions on macOS and Linux and must not be served as
public content or included in diagnostics. A malformed, truncated, unsupported
or over-limit state file makes startup fail without replacing the file; fix the
path or restore a known-good copy instead of silently losing rooms.

Run server-only and Unreal end-to-end checks:

```sh
scripts/run_dedicated_server_smoke.sh
unreal/Catan/Scripts/run_dedicated_e2e.sh
unreal/Catan/Scripts/run_android_dedicated_e2e.sh \
  unreal/Catan/Builds/AndroidLatest/Catan-arm64.apk
```

The Android E2E starts two virtual phones, connects both through the host
machine's `10.0.2.2` gateway, drops the first create response, completes initial
placement and normal turns, then restarts the server and verifies both clients
recover the persisted game. Logs and post-reconnect screenshots from both
clients are retained under `/tmp`.

The wire protocol is newline-delimited request/response TCP. Legacy V1 verbs
remain accepted. UE clients use `CREATE2`, `JOIN2`, `READY2`, `START2`,
`COMMAND2` and `LEAVE2`, each carrying a random request ID. Successful replies
are cached in the private server state and an identical retry returns the
original result—even after restart—without applying the mutation again. Reuse
of an ID with a different operation or payload is rejected. Clients use the
same ID for up to three bounded network attempts; connect, send and receive
each have a three-second deadline. `RESUME` and `SNAPSHOT` are read-only.

`catan-dedicated-probe` is provided for diagnostics and smoke automation. The
test-only `--drop-response-once OPERATION` option applies and persists the first
matching request but closes its connection before replying, allowing an E2E to
prove the lost-response path.

## Hidden victory cards

Before the game finishes, snapshots expose only construction and award victory
points for opponents. Victory-point development cards and their points are
returned only in the authenticated owner's private snapshot. At `Finished`,
total points and victory-card counts are revealed for every player so the final
dashboard can explain the result.

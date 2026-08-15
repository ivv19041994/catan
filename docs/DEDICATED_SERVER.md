# Dedicated server

The dedicated server is a portable, headless C++ process. It owns one
`GameController` per lobby and runs on macOS and Linux; Unreal Engine is not
required on the server machine.

Build and run:

```sh
cmake -S . -B build -DCATAN_BUILD_CONSOLE=OFF
cmake --build build --target catan-dedicated-server
./build/catan-dedicated-server --bind 0.0.0.0 --port 17777
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

Run server-only and Unreal end-to-end checks:

```sh
scripts/run_dedicated_server_smoke.sh
unreal/Catan/Scripts/run_dedicated_e2e.sh
```

The wire protocol is a version-one request/response TCP protocol with one
newline-terminated request per connection. `catan-dedicated-probe` is provided
for diagnostics and smoke automation.

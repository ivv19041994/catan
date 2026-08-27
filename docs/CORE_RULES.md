# Core rules and module boundary

The repository-level `catan_engine` CMake target is the authoritative and
standalone rules module. It has no Unreal Engine, UI, rendering, networking, or
platform dependencies. Unreal compiles the same sources and adapts public Core
values into `FCatanGameView`; clients can only submit commands and consume
snapshots.

## Resource bank

`ResourceBank` owns 19 physical cards of each resource. Starting resources,
dice production, Year of Plenty, and bank trades take cards from it. Building,
development-card costs, bank-trade payment, and seven discards return cards.
Player-to-player trades, Monopoly, and robber theft do not change bank totals.

On production shortage:

- a sole recipient receives every remaining card of that type;
- if several players are owed that type and the bank cannot satisfy everyone,
  none of them receives it.

The five counts are public information. Unreal renders them as five coloured
3D piles opposite the dice; pile height and the number above it follow the Core
count. LAN replication and the dedicated protocol transport the same public
counts.

## Development cards

One action development card may be played during the owner's turn, including
before rolling. A card bought this turn remains unavailable until the next
turn. Knight returns to the phase from which robber movement started. Monopoly
and Year of Plenty leave the current phase unchanged. Road Building places up
to two legal free roads and returns to its original Roll Dice or Common Play
phase. It cannot be consumed when no legal road can be placed, and ends early
when no second road or piece is available.

Year of Plenty is atomic: both requested physical cards must be present before
the development card is consumed.

## Victory and trade validation

A player with ten points wins only during their own turn. The check also runs
when a new current player's turn begins, before rolling. Private victory-point
cards remain hidden from opponents until the final state.

Bank trades require two different real resources. Player offers require
positive quantities on both sides, reject `Resurse::Not`, and cannot contain
the same resource on both sides. Publishing an offer never probes the target's
private hand; affordability is checked only when the recipient accepts.

## Regression suites

Rules are split into domain folders under `tests/`: `resource_bank`,
`production`, `setup`, `development_cards`, `trade`, `awards_victory`, and
`persistence`. Dedicated snapshot transport is covered by
`tests/dedicated_server`. Build and run them through CMake/CTest.

# Bot AI: ten additional iterative four-player games

Date: 2026-08-28. This continues the first five-game experiment with ten counted games,
iterations 6 through 15. Every game used four bot-controlled players in headless autoplay.
Raw logs are in `/tmp/catan-bot-ten-more-iterations/iteration-{6..15}.log`.

An interrupted iteration-8 process completed after the user added the Monopoly-port
requirement. It was renamed to `iteration-8-pre-port.log` and deliberately excluded; the
counted iteration 8 was rerun after the requested change.

| Iteration | Actions | Rolls | Final VP | Finding and next change |
|---|---:|---:|---|---|
| 6 | 444 | 109 | 10/5/5/7 | 4/29 trades accepted; reject low-confidence recipients |
| 7 | 476 | 108 | 8/10/7/9 | Monopoly read private hands; replace with public production, total cards, and caller ports |
| 8 | 327 | 80 | 10/5/6/3 | Flat specialized-port bonus; value settlement ports against actual production |
| 9 | 359 | 86 | 5/5/5/10 | Five-settlement bot had no city plan; force piece-aware city goal |
| 10 | 475 | 106 | 9/5/6/10 | `bHasCityTarget` meant affordable now, not structurally possible; split location from action availability |
| 11 | 268 | 59 | 5/10/4/5 | Structural planning removed all development purchases; restore late/army diversification |
| 12 | 402 | 86 | 6/10/6/8 | One bot spent all 15 roads; cap ordinary network by owned buildings |
| 13 | 313 | 70 | 7/10/5/6 | Mature network lacked ore; prioritize missing wheat/ore production |
| 14 | 453 | 100 | 8/7/10/5 | Repeated rejected offers; add finite signature cooldown |
| 15 | 461 | 95 | 9/9/10/9 | Final validation; cooldown blocked 11 immediate repeats |

## Result

The final game had the strongest competitive table of all fifteen iterations: every bot
finished on match point or won. The winner used four settlements and three cities. The
other plans remained viable: two players reached four cities, while another combined five
settlements, a city, and Longest Road.

The experiment does not claim monotonic action-count improvement: map layout, dice, and
development deck remain random. It does show that each identified failure mode is now an
explicit policy with a regression test:

- Monopoly uses only public production estimates and public card totals, plus the caller's
  own 2:1/3:1/4:1 port rates; it no longer reads opponent resource composition.
- Specialized settlement ports are matched to the bot's production portfolio and no
  longer distort city-upgrade selection.
- Planning distinguishes future legal locations from actions affordable right now.
- Development cards and cities coexist instead of one strategy starving the other.
- Non-tactical road growth is bounded; award-winning and award-defending roads bypass it.
- Mature settlement networks cover missing wheat/ore instead of blindly duplicating pips.
- Rejected identical offers have a temporary cooldown, while changed offers remain legal.

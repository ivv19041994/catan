# Bot AI: five iterative four-player games

Date: 2026-08-28. Each cycle ran one headless game with four bot-controlled players
(`-CatanAutoBots=3 -CatanBotAutoplay`). Raw Unreal logs were retained locally in
`/tmp/catan-bot-five-iterations/iteration-{1..5}.log`.

Because the production map, dice, and development deck are random, action counts from a
single game are diagnostic rather than a statistically controlled benchmark. The useful
signals are repeated low-value decisions, conversion of resources into points, trade
acceptance, and whether the winner uses a coherent path to victory.

| Iteration | Winner | Bot actions | Rolls | Observed weakness | Improvement for next game |
|---|---:|---:|---:|---|---|
| 1 | Iteration1 | 482 | 105 | 49 road-target states and no semantic decision trace | Add action/result traces; require a normal road budget while preserving award/win/denial roads |
| 2 | Bot 3 | 563 | 132 | 51 development-card purchases, only 3 cities | Preserve hands within two cards of a city or one card of a settlement |
| 3 | Bot 1 | 435 | 97 | 34 player offers: 9 accepted, 25 rejected | Choose recipients from public board production; never inspect private resource composition |
| 4 | Bot 2 | 329 | 76 | Largest-Army holder still spent every ready knight before rolling | Hold a safe knight; play for own blocked production or against an 8+ VP threat |
| 5 | Bot 1 | 519 | 120 | Final validation: three players finished at 9/10/9 VP | No further change in this five-cycle experiment |

## Decision-quality results

- Ordinary roads now require either a funded expansion hand or an exact tactical reason.
  `IsTacticalRoad` explicitly preserves claiming or defending Longest Road, including a
  winning road or taking two points from a match-point opponent.
- After the development-card guard, iteration 3 used 27 purchases instead of 51 and built
  8 cities instead of 3. Its game ended in 435 actions instead of 563.
- Public-production trade targeting improved the next game's acceptance from 9/34 (26%)
  to 9/19 (47%) and the game ended in 329 actions. It uses visible buildings, hex numbers,
  total card count, and VP only—not an opponent's private resource composition.
- The fifth game was slower due to random state, but its 9/10/9/5 final table was more
  competitive than iteration 4's 5/5/10/8. The winner combined settlements, cities, and
  roads; the other two contenders pursued Longest Road and Largest Army/cities.

## New diagnostics

- `CATAN_BOT_DECISION`: actor, phase, plan, VP, hand, action, and target.
- `CATAN_BOT_TRADE_RESPONSE`: offerer, recipient, and accept/decline result.
- `CATAN_BOT_RESULT`: final VP, resources, development cards, buildings, roads, knights,
  Longest Road, and Largest Army for every player.

The strategy changes are covered by automation tests for ordinary versus tactical road
funding, match-point road denial, development-card opportunity cost, fair trade targeting,
and knight timing.

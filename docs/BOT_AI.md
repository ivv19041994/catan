# Bot AI

The production evaluator uses the exact two-dice distribution: numbers 6/8
have five production pips, 5/9 four, 4/10 three, 3/11 two, and 2/12 one.
Resource scarcity on the current board modifies value, while diversity remains
a smaller tie-breaker instead of overwhelming expected production.

The bot derives a strategic plan from its owned production:

- strong wood/clay favours expansion and settlement routes;
- strong wheat/ore/sheep favours cities, development cards, and Largest Army;
- a mixed portfolio keeps a balanced plan.

Road selection is graph based. Each candidate is evaluated against reachable
legal settlement sites up to two additional roads away, the site's expected
production, competition from opponent roads, and the candidate's actual change
to the longest continuous trail. A paid road without a settlement, blocking,
or award purpose is skipped. Initial roads and already-committed Road Building
cards remain mandatory.

The design follows the official dice/production rules and established Catan AI
work on domain-aware heuristics and bounded search:

- [CATAN base game and roll probabilities](https://www.catan.com/catan)
- [Playing Catan with Monte Carlo Tree Search](https://catan-ai.github.io/project/)
- [Cross-dimensional neural networks for Catan](https://arxiv.org/abs/2008.07079)

The current implementation deliberately uses deterministic expected values
rather than random rollouts: for the short road/placement horizon this gives a
stable result at a much lower mobile CPU cost. The state evaluator and topology
planner are suitable inputs for a future bounded MCTS layer.

Run deterministic strategy tests and full-game tournaments with:

```bash
unreal/Catan/Scripts/run_bot_tournament.sh 3
unreal/Catan/Scripts/run_bot_e2e.sh
```

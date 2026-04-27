# Alpha-Mu: A Bridge Player's Guide

## What is alpha-mu?

Alpha-mu is a way of analyzing bridge play when you do not know where all the
cards are. It is the kind of thinking a declarer does at the table:

> "I can see my hand and dummy, but I don't know which defender holds
> the queen of spades. Let me consider all the plausible distributions
> and figure out which play works best across those possibilities."

Alpha-mu formalizes that reasoning into a computer search. It considers many
**possible worlds** — each one a different way the unseen cards might be
distributed — and finds the play that performs best when all those worlds are
taken into account.

## How does it differ from DDS?

DDS (double-dummy solver) computes "perfect" results — the number of tricks
each side can take when all four hands are visible. Alpha-mu answers a different
question:

| | DDS | Alpha-mu |
| --- | --- | --- |
| **Sees** | All four hands | Only declarer's hand and dummy |
| **Assumes** | Perfect play by both sides | Best play under uncertainty |
| **Answer** | "With perfect play, declarer takes 10 tricks" | "Considering what declarer can know, the best play is the 7 of hearts" |

Alpha-mu **uses** DDS internally as a leaf evaluator. When alpha-mu needs to
know how many tricks a specific card layout produces, it hands that layout to
DDS. But alpha-mu reasons across many such layouts simultaneously.

## What are "possible worlds"?

A **possible world** is one complete assignment of the hidden cards to the
two defenders. Alpha-mu generates many candidate worlds and filters them using
everything declarer could know at the decision point:

1. **Visible cards**: declarer's hand and dummy
2. **Cards already played**: which cards appeared on earlier tricks
3. **Follow-suit evidence**: if a defender failed to follow suit, they hold no
   more cards in that suit
4. **Bidding evidence**: auction-derived constraints on shape or strength
5. **Deduplication**: equivalent worlds are collapsed to save search time

## What does the search do?

At each point in the play:

- **Declarer's turn (Max node)**: alpha-mu asks which play is best across all
  worlds simultaneously
- **Defender's turn (Min node)**: each defender plays best in their own actual
  world

Alpha-mu tracks per-world outcomes using **Pareto fronts** — the set of plays
where no other play beats them in every world.

## What does the output mean?

- **Recommended move**: the play with the highest **mu** (average tricks)
- **Alternative moves**: other legal plays and their mu values
- **World summary**: how many worlds were generated, filtered, and searched
- **Per-world outcomes**: tricks per candidate play in each world
- **Plausibility**: soft likelihood weights (when available)

## How to read a decision-point analysis

1. **Check the world count**: fewer surviving worlds means a more constrained
   (stronger) analysis
2. **Compare recommended vs actual play**: mu difference shows significance
3. **Check per-move mu values**: similar mu means the recommendation is weak
4. **Read world explanations**: understand what drove the filtering

## Limitations

- **Depth**: search looks ahead 2-4 half-tricks (deeper is slower)
- **World count**: at most 64 worlds searched simultaneously
- **Speed**: ~30-65 s/board at depth 2, ~4000-7500 s/board at depth 3 on M1 Max
- **Defender model**: defenders assumed to play perfectly in their own world

## Terminology

| Term | Meaning |
| --- | --- |
| **World** | One possible distribution of the hidden cards |
| **Outcome vector** | Tricks declarer makes in each world for a given play |
| **Pareto front** | Set of non-dominated outcome vectors |
| **mu** | Average tricks across surviving worlds |
| **Max node** | Declarer's choice point |
| **Min node** | Defender's choice point |
| **DDS leaf** | Position evaluated by DDS for exact results |
| **World cut** | Dropping a world whose outcome cannot affect the result |
| **Plausibility** | Soft weight reflecting how likely a world is |

## Further reading

- [Alpha-mu and DDS](alpha-mu.md) — technical overview
- [Architecture](architecture.md) — solver structure
- [Information-state contract](alpha-mu-information-state.md) — world inputs

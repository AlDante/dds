# Alpha-Mu Implementation Plan
## Current status
The groundwork completed before algorithmic changes includes:
- stronger regression comparisons in `test/compare.cpp`,
- a focused public-API regression harness in `test/regression_api.cpp`,
- a standalone `Timer` regression harness in `test/timer_regression.cpp`,
- warning cleanup needed to build and test reliably on the current macOS toolchain,
- default macOS multi-threading through GCD + STL instead of Boost.
That means the next alpha-mu work can proceed against a materially better-tested baseline than before.
## Phase 0 — completed preparation
The goal of the preparation phase was to ensure that changes to root search policy can be measured and verified.
Completed ingredients include:
- golden-data regression for solve / calc / play / par / dealer-par,
- API-consistency checks for tables, par APIs, conversion APIs, and exact-score probe behavior,
- platform build cle# Alpha-Mu Implementation Plan
## Current sta## Phase 1 — next step
### Target
Refactor the duplicated root exact-score probing logic in- stronger regression c### Scope
Touch only the root driver lo- a focused public-API regress- `SolveSameBoard()`
- `Ana- a standalone `Timer` regression harness in `test/timer_regress- guess,- warning cleanup needed to build and test reliably ointo a single helper - default macOS multi-threading through GCD + STL instead of Boost.
That means th- `That means the next alpha-mu work can proceed against a materia- TT ## Phase 0 — completed preparation
The goal of the preparation phase was to ensure that change- rerunThe goal of th- benchmark representatCompleted ingredients include:
- golden-data regression for solve / calc / play / par / dealer-par,
- ASuggested - golden-data re- `hands/list10- API-consistency checks for- `hands/thomas1.txt`
- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work only if justified
If the root refacto## Current sta## Phase 1 — next step
### - invest### Target
Refactor the duplicated rop`Refactor llTouch only the root driver lo- a focused public-API regress- `SolveSameBoard()`
- `Ana- rk- `Ana- a standalone `Timer` regression harness in `test/timer_regress- guess,fiThat means th- `That means the next alpha-mu work can proceed against a materia- TT ## Phase 0 — completed preparation
The goal of the preparation phase was to ensure that change- rerunThe goal of th- benchmark repregeThe goal of the preparation phase was to ensure that change- rerunThe goal of th- benchmark representatCompleted ingredje- golden-data regression for solve / calc / play / par / dealer-par,
- ASuggested - golden-data re- `hands/list10- API-consistency cun- ASuggested - golden-data re- `hands/list10- API-consistency checks
- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work o- [`../INSTALL`If the root refacto## Current sta## Phase 1 — next step
### - invest### Target
Refactor the nt### - invest### Target
Refactor the duplicated- [`../exampRefactor the duplicatle- `Ana- rk- `Ana- a standalone `Timer` regression harness in `test/timer_regress- guess,fiThat means th- `That s documeThe goal of the preparation phase was to ensure that change- rerunThe goal of th- benchmark repregeThe goal of the preparation phase was to ensure that change- rerunThe goal of th- benchmark representatComplets,- ASuggested - golden-data re- `hands/list10- API-consistency cun- ASuggested - golden-data re- `hands/list10- API-consistency checks
- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work o- [`../INSTALL`If the root refacto## Current sta## Phase 1 — next step
#er- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work o- [`../IN5. `alpha-mu.md`
6. `implementation-plan.md`
7### - invest### Target
Refactor the nt### - invest### Target
Refactor the duplicated- [`../exampRefactor the duplicatle- `Ana- rk- `Ana- a standalone ryRefactor the- current architecture guidance,
- current implem- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work o- [`../INSTALL`If the root refacto## Current sta## Phase 1 — next step
#er- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work o- [`../IN5. `alpha-mu.md`
6. `implementation-plan.md`
7### - invest### Target
Refactor the nt### - invest### Target
Refactor the duplicated- [`../exampRefactor the duplicatle- `Ana- rk- `Ana- a standalone ryRefactor the- current architecture guidance,
- current implem- `hands/thomas2.tLE#er- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work o- [`../IN5. `alpha-mu.md`
6. `implementation-plan.md`
7### - invest#O
6. `implementation-plan.md`
7### - invest### Target
Refactor the nt### - invest### TaEXTRACT_STATIC         7### - invest### Target
Re  Refactor the nt### - iS Refactor HIDE_UNDOC_CLASSES     = NO
H- current implem- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work o- [`../INSTALL`If the root refacto## Cu  #er- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work o- [`../IN5. `alpha-mu.md`
6. `implementation-plan.md`
7### - invest### Target
Refact  6. `implementation-plan.md`
7### - invest### Target
Refactor the nt### - invest### Target
Refactor the d    7### - invest### Ta  ../doc/Refactor the nt### - id Refactor the duplicated- [`../exampRl-- current implem- `hands/thomas2.tLE#er- `hands/thomas2.tx- platform build cle# Alp## Phase 3 — deeper search work o- [`../IN5. `alpem6. `implementation-plan.md`
7### - invest#O
6. `implementation-plan.md`
7### - invest### Target
Refactor the nt### - invest### TaEXTRACT_STATI\
7### - invest#O
6. `implem./6. `implementa \7### - invest### Target
Re..Refactor the nt###      Re  Refactor the nt### - iS Refactor HIDE_UNDOC_CLASSES     = NO
H- current imrTH- current implem- `hands/thomas2.tx- platform build cle# Alp##  6. `implementation-plan.md`
7### - invest### Target
Refact  6. `implementation-plan.md`
7### - invest### Target
Refactor the nt### - invest### Target
Refactor the d    7### - invest### Ta  ../doc/Refactor the nt### - id Refactor the duplicatedES7### - invest### Target
ReYEReREFERENCES_RELATION   7### - invest### Target
Refactor tS
Refactor the nt### - i = YES
HTML_OUTPUT            = html
GEN7### - invest#O
6. `implementation-plan.md`
7### - invest### Target
Refactor the nt### - invest### TaEXTRACT_STATI\
7### - invest#O
6. `implem./6. `implementa \7### - invest### Target
Re..Refactor the nt###      Re  Refactor the nt### - iS Refactor HIDE_UNDOC_CLASSES   EF6. `imTAB_SIZE  7### - invest### BUILTIN_STLRefactor the nt###CLASS_7### - invest#O
6. `imCOLLABORATION_GRAPH    = N6.GROUP_GRAPHS  Re..Refactor the nt###      Re  Refactor tDOT_IMAGE_H- current imrTH- current implem- `hands/thomas2.tx- platform build cle# Alp##  6. `impleme" 7### - invest### Target



python3 - <<'PY'
from pathlib import Path
base = Path('/Users/david/Documents/dev/CLionProjects/dds/docs')
base.mkdir(exist_ok=True)
files = {
    'README.md': '''# DDS Documentation
This directory contains the maintained documentation entry point for the DDS codebase.
It combines:
- curated Markdown guides for architecture and implementation work,
- generated Doxygen pages for selected public and internal headers,
- links to the historical material already present in `../doc/`.
## Prerequisite
Install `doxygen` and ensure it is available on your `PATH`.
## Build
From the repository root:
```sh
make -C docs html
```
Or from inside this directory:
```sh
make html
```
The generated HTML site will be written to:
```text
build/html/index.html
```
## Clean
```sh
make -C docs clean
```
## Contents
The generated site includes:
- `mainpage.md` — landing page
- `architecture.md` — high-level DDS architecture
- `api-overview.md` — key public APIs and usage patterns
- `data-structures.md` — important internal and public data structures
- `alpha-mu.md` — alpha-mu background and DDS integration notes
- `implementation-plan.md` — staged implementation roadmap
- `legacy-docs.md` — guide to the historical documentation in `../doc/`
Selected existing Markdown documentation from the repository is also included in the Doxygen input set.
''',
    'Makefile': 'DOXYGEN ?= doxygen\nDOXYFILE := Doxyfile\n\n.PHONY: html clean\n\nhtml:\n\t@command -v "$(DOXYGEN)" >/dev/null 2>&1 || { \\\n\t\techo "doxygen not found on PATH"; \\\n\t\texit 1; \\\n\t}\n\t$(DOXYGEN) $(DOXYFILE)\n\nclean:\n\trm -rf build\n',
    'mainpage.md': '''# DDS Documentation
DDS is a bridge double-dummy solver wi```text
build/html/index.html
```
## Cleanllbuild/ t```
## Clean
```sh
mer##or```sh
m `src/`,```
## Contents
Ten##maThe generaru- `mainpage.md` — landingst- `architecture.md` — high-leun- `api-overvThis documentation set combines curated - `data-structures.md` — important internal and publicead- `alpha-mu.md` ? [Architecture](architecture.md)
- [Key data structures](data-structures.md)
- [API overview](api-overview.md)
- [Alpha-m- `legacy-docs.md` — guide to the historical documentatioioSelected existing Markdown documentation from the repository is also incce''',
    'Makefile': 'DOXYGEN ?= doxygen\nDOXYFILE := Doxyfile\n\n.PHONY: html clean\n\nhtml:\n\t@comm/S   er    'mainpage.md': '''# DDS Documentation
DDS is a bridge double-dummy solver wi```text
build/html/index.html
```
## Cleanllbuild/ t```
## Clean
```sh
mer##or```sh
m `src/`,```
## Contents
Ten##maThe generaru- `mainpage.md` — landingst- `architecture.md` ?sDDS is a b- Per-thread memory and state: `build/html/ind- Parallel execution and schedul```
## Cleanllbuild/ `##c/## Clean
```s `src/Ini```sh
m- Tests m `src/`,``ion harnesses: Ten##maT## S- [Key data structures](data-structures.md)
- [API overview](api-overview.md)
- [Alpha-m- `legacy-docs.md` — guide to the historical documentatioioSelected existing Markdown documentation from the repository is also incce''',
    'Makefileng- [API overviewThe historical files in `doc/- [Alpha-m- `legacynd are included    'Makefile': 'DOXYGEN ?= doxygen\nDOXYFILE := Doxyfile\n\n.PHONY: html clean\n\nhtml:\n\t@comm/## High-level view
DDS is organized as a layered solDDS is a brid1. **Public API layer**
   - `include/dll.h`
   - Exposes C-compatible entry points and public data structures.
2. **Root orchestratibuild/html/   - `src/SolverIF.cpp`
   - `src/CalcTables.cpp`
   - `src/P## Clean
```sh
me   - ```sh
malerPar.cpp`, `src/Pa## Cont   - CTen##maTheI ## Cleanllbuild/ `##c/## Clean
```s `src/Ini```sh
m- Tests m `src/`,``ion harnesses: Ten##maT## S- [Key data stru   - `src/QuickTricks.cpp`
   - `src/LaterTricks.c```s   - Performs threshold-stym- Tests m `src/`ch- [API overview](api-overview.m4. **State, memory, and caching**
   - `src/dds.h`
   - `src/Memory.h`
   - `src/TransTable.h`    'Makefileng- [API overviewThe historica   - Holds per-thread state, search position data, and transposition-table caches.
5. **Parallel execution DDS is organized as a layered solDDS is a brid1. **Publ   - `src/Scheduler.h`, `src/Scheduler.cpp`
   - `src/Init.cpp`
   - Configures thread backends, allocates per-thread memory, and groups jobs for throughput.
6. **Tests and examples**
   - `test/`
   - `examples/`
   - Validate correctness and show public API usage.
## Solver flow for a single board
A typical `SolveBoard` / `SolveBoardPBN` request flows as follows:
1. Public caller fills a `deal` or `dealPBN` structure.
2. `SolveBoard` validates parameters and resolves the target thread context.
3. `SolveBoardInternal()` in `src/SolverIF.cpp`:
   - classifies the deal,
   - initializes `ThreadData`,
   - rebuilds deal tables when necessary,
   - runs root-level exact-score / threshold probing,
   - collects winning moves into `futureTricks`.
4. `ABsearch*()` in `src/ABsearch.cpp` recur5. **Parallel execution DDS is organized as a 5. `Moves`, `QuickTricks`, and `LaterTricks` prune the search and maintain move ordering.
6. `TransTabl   - `src/Init.cpp`
   - Configures thread backends, allocates per-th## Why DDS is already close to alpha-mu
DDS is not structured as a plain s6. **Tests and examples**
   - `test/`
   - `examples/`
   - Validatanswers a boolean-style question:
- can the side to move force at   - Validate cy ## Solver flow for a single board
A typical `SolveBt A typical `SolveBoard` / `SolveBot1. Public caller fills a `deal` or `dealPBN` structure.
2. `Solvepl2. `SolveBoard` validates parameters## Major subsystems
3. `SolveBoardInternal()` in `src/SolverIF.cpp`:
   - classifies the d- singl   - classifies the deal,
   - initializes `Thrbu   - initializ- par calcul   - rebuilds- play-analysis ca   - runs root-level exact-score / thresls   - collects winning moves into`src/dds.h` contains t4. `ABsearch*()` in `src/ABsearch.cpp` recur5. ` 6. `TransTabl   - `src/Init.cpp`
   - Configures thread backends, allocates per-th## Why DDS is already close to alpha-mu
DDS is not structured as a plain s6. **Tests and exampleea   - Configures thread backendsllDDS is not structured as a plain s6. **Tests - current position buffers,
- transposition table handle,
- move generator,
- best-move history,
- timing / stats objects when enabled.
### Recursive search
`src/ABsearch.cpp` contains four specialized search entry points:
- `ABsearch`
- `ABsearch1`
- `ABsearch2`
- `ABsearch3`
These are specialized by the relative hand position inside the trick, avoiding a more generic but slower uniform recursion pattern.
### Pruning and proof helpers
DDS relies heavily on bridge-specific pruning helpers:
- `QuickTricks` identifies immediate forcing w   - Configures thread backends, allocates per-th## Why DDS is already close to alpha-mu
DDS is not structured as a plain s6. **Tests and exampleea   - Configures thread backendsllDDS is not structured as a plain s6. **Tests - current position buffers,
- treDDS is not structured as a plain - best move,
- least-winning-rank metadata.
There are tw- transposition table handle,
- move generator,
- best-move history,
- timing / stats objects when enabled.
### Recursive search
`src/ABsearch.cpp` contains four d - move generator,
- best-move - best-move histow- timing / st- GCD (`### Recursive sear- STL threads (`DDS_T`src/ABsearch.cpp` ch- `ABsearch`
- `ABsearch1`
- `ABsearch2`
- `ABsearch3`
These areuc- `ABsearchwo- `A## Current- `ABsearch3alThese are spr ### Pruning and proof helpers
DDS relies heavily on bridge-specific pruning helpers:
- 2. preserve the bridge-specific pruning logicDDS relies heavily on brion-ta- `QuickTricks` ident4. improve root search policy firsDDS is not structured as a plain s6. **Tests and exampleea   - Configures thread backendsllDDS is not structured as a plain s6. **Tes(`- treDDS is not structured as a plain - best move,
- least-winning-rank metadata.
There are tw- transposition table handle,
- move generator,
- best-move history,g - least-winning-rank metadata.
T  - PBN-string reprThere are tw- transpositio- `fu- move gener  - Result structure returned by solve functions-   - Contains candid### Recursive search
`src/ABsearch.cpte`src/ABsearch.cpp`###- best-move - best-move histow- timing / st-, `boards- `ABsearch1`
- `ABsearch2`
- `ABsearch3`
These areuc- `ABsearchwo- `A## Current- `ABsearch3alThese are spr ### Pruning anal- `ABsearch2ea- `ABs  - InpuThese areuc--dDDS relies heavily on bridge-specific pruning helpers:
- 2  - Per-deal and batched table results- 2. preserve the bridge-specific pruni- `parResults`
 - least-winning-rank metadata.
There are tw- transposition table handle,
- move generator,
- best-move history,g - least-winning-rank metadata.
T  - PBN-string reprThere are tw- transpositio- `fu- move gener  - Result structure returned by solve functions-   - Contains candid### Recursive search
`src/ABsearch.cpte`siThe### Runtime and configuratio- move generator,
- best-  - Describes the- best-move histe T  - PBN-string reprThere are     - platform,
    - c`src/ABsearch.cpte`src/ABsearch.cp    - thread count,
    - memory/thread sizing.
## Internal solver structures (`src/dds.h`)
### `pos`
`pos` is the core recursive position object used during search.
Important fields include:
- `rankInSuit[DDS_HANDS][DDS_SUITS]`
  - Remaining card bitse- 2  - Per-deal and batched table results- 2. preserve the bridge-specific pruni- `parResults`
 - least-winning-rank metadata.
There are tw- transposition table handle,
- move gdD - least-winning-rank metadata.
There are tw- transposition table handle,
-- `winRanks[50][DDS_There ar  - Winning-rank masks b- move generator,
- bes  - Leader for each- best- `move[50]`T  - PBN-string reprThere are tw- transpositio-   - T`src/ABsearch.cpte`siThe### Runtime and configuratio- move generator,
- best-  - Describes the- best-move histe T  - PBN-string reprTher### `moveType`
Re- best-  - Describes the- best-move histe T  - sequence information,
-    - c`src/ABsearch.cpte`src/ABsearch.cp    - thread count,
    - memory/thread sizind     - memory/thread sizing.
## Internal solver structures (#### Internal sol`ThreadData` ### `pos`
`pos` is the core recursive posihr`pos` isl Important fields include:
- `rankInSuit[DDS_HA  - The active `po- `rankInSu- `transTable`
  - Remaining card bitse- 2  - Per-ck - least-winning-rank metadata.
There are tw- transposition table handle,
- move gdD - least-winning-rank me- `bestMoveThere are tw- tr  - Root and TT-- move gdD - least-winning-rank- `nodes`, There are tw- transposition table - `rel`
  - Large precomputed equivalence data.
Th- bes  - Leader for each- best- `move[50]`T  - PBN-string reprThere ahm- best-  - Describes the- best-move histe T  - PBN-string reprTher### `moveType`
Re- best-  - Describes the- best-move histe T  - sequence informat- `ubound`
- `lbouRe- best-  - Describe- `bestMoveRank`
- `leastWin[DDS_SUITS]`
For alpha-mu planni-    - c`src/ABsearch.cpte`src/ABsearch.cp    - thread count,
    - mst    - memory/thread sizind     - memory/thread sizing.
## Ined## Internal solver st- `TransTableL`
This abstraction i`pos` is the core recursive posihr`pos` isl Important fields include:`s- `rankInSuit[DDS_HA  - The activReturned by the scheduler when a work  - Remaining card bitse- 2  - Per-ck - least-winning-rank### ScThere are tw- transposition table handle,
- move gdD - least-winnints- move gdD - least-winning-rank me- `besTh  - Large precomputed equivalence data.
Th- bes  - Leader for each- best- `move[50]`T  - PBN-string reprThere ahm- best-  - Describes the- best-move histe T  -trTh- bes  - LeaderAll exported functions Re- best-  - Describes the- best-move histe T  - sequence informat- `ubound`
- `lbouRe- best-  - Describe- `bestMoveRank`
- - `SetMaxThreads(int userThrea- `lbouRe- best-  - Describe- `b- `SetResources(int maxMemoryMB, int maxThrea- `leastWin[DDS_SUITS]- `GetDDSInfo(DDSInfo *For alph- `ErrorMessage(    - mst    - memory/thread sizind     - memory/thread sizing.
## Ined## Interre## Ined## Internal solver st- `TransTableL`
This abstraction i` This abstraction i`pos` is the core recursde- move gdD - least-winnints- move gdD - least-winning-rank me- `besTh  - Large precomputed equivalence data.
Th- bes  - Leader for each- best- `move[50]`T  - PBN-string reprThere ahm- best-  - Describes the- best-move histe T  -trTh- bes  - LeaderAll expoTypical usTh- bes  - Leader for each- best- `move[50]`T  - PBN-string reprThere ahm- best-  - Describes the- best-mov a- `lbouRe- best-  - Describe- `bestMoveRank`
- - `SetMaxThreads(int userThrea- `lbouRe- best-  - Describe- `b- `SetResources(int maxMemoryMB, int maxThrea- `leastWin[DDS_SUITS]- `GetDDSInfo(DDSInfo *For alph- `ErrorMessage(    - mst    ti- - `SetMaxThreads(int userThrea- `lbouRe- io## Ined## Interre## Ined## Internal solver st- `TransTableL`
This abstraction i` This abstraction i`pos` is the core recursde- move gdD - least-winnints- move gdD - least-winning-rank me- `besTh  - Large precomputed equivalence data.
Th- beheThis abstraction i` This abstraction i`pos` is the cor## Par Th- bes  - Leader for each- best- `move[50]`T  - PBN-string reprThere ahm- best-  - Describes the- best-move histe T  -trTh- bes  - LeaderAll expoTypical usTh- bes  - Lead..- - `SetMaxThreads(int userThrea- `lbouRe- best-  - Describe- `b- `SetResources(int maxMemoryMB, int maxThrea- `leastWin[DDS_SUITS]- `GetDDSInfo(DDSInfo *For alph- `ErrorMessage(    - mst    ti- - `SetMaxThreads(int userThrea- `lbouRe- io## Ined## Interre## Ined## Internal solver st- `TransTableL`
This abstlPThis abstraction i` This abstraction i`pos` is the core recursde- move gdD - least-winnints- move gdD - least-winning-rank me- `besTh  - Large precomputed equivalence data.
Th- beheThis abstraction i` This abstraction i`pos` is the cor## Par Th- bes  - Leader for each- best- `move[50]`T  - PBN-stDDTh- beheThis abstraction i` This abstraction i`pos` is the cor## Par Th- bes  - Leader for each- best- `move[50]`T  - PBN-string reprThere ahm- best-  - Describes the- best This abstlPThis abstraction i` This abstraction i`pos` is the core recursde- move gdD - least-winnints- move gdD - least-winning-rank me- `besTh  - Large precomputed equivalence data.
Th- beheThis abstraction i` This abstraction i`pos` is the cor## Par Th- bes  - Leader for each- best- `move[50]`T  - PBN-stDDTh- beheThis abstraction i` This abstraction i`pos` is the cor## Par Th- bes  - Leader for each- best- `move[50]`T  - PBN-string reprThere ahm- best-  - Describes the- best This abstlPThis abstraction i` This abstraction i`pos` igiTh- beheThis abstraction i` This abstraction i`pos` is the cor## Par Th- bes  - Leader for each- best- `move[50]`T  - PBN-stDDTh- beheThis abstraction i` This abstraction i`pos` is tn Th- beheThis abstraction i` This abstraction i`pos` is the cor## Par Th- bes  - Leader for each- best- `move[50]`T  - PBN-stDDTh- beheThis abstraction i` This abstraction i`pos` is the cor## Par Th- bes  - Leader for each- best- `move[50]`T  - PBN-string reprThere ahm- best-  - Describes the- best This abstlPThis abstraction i` This abstraction i`pos` igiTh- beheThis abstraction i` This abstraction i`pos` is the cor## Par Th- bes  - Leader for each- best- `move[50]`T  - PBN-sttion can be evolutionary rather than revolutionary.
## What should stay stable initially
The following components are high value and already deeply integrated:
- `src/Moves.cpp`
- `src/QuickTricks.cpp`
- `src/LaterTricks.cpp`
- the four specialized `ABsearch*()` functions
- TT storage semantics based on **remaining tricks from the node**
These should not be rewritten in the first phase.
## Where alpha-mu should begin
The first practical insertion point is the **root exact-score logic** in `src/SolverIF.cpp`.
That code already maintains concepts such as:
- guess,
- lower bound,
- upper bound,
- repeated threshold probes.
A clean alpha-mu-style refactor can centralize that logic into a helper while keeping the existing recursive proof engine intact.
## Why not start inside `ABsearch0()`?
Because that would combine several risks at once:
- recursive control flow changes,
- TT interaction changes,
- move-order / proof interaction changes,
- benchmark and correctness uncertainty.
The project already has a strong thr## What should stay stable initially
The followi tThe following components are high v s- `src/Moves.cpp`## Constraints for a correct DDS integration
A good al- `src/QuickTricon- `src/LaterTricks.cppol- the four speciali1. **- TT storage semantics based on **remain2. **MoThese should not be rewritten in the first phase.
## 3. **TT bounds## Where alpha-mu should begin
The f4. **Repeated The first practical insertionatThat code already maintains concepts such as:
- guess,
- lower bound,
- up## Practical succes- guess,
- lower bound,
- upper bound,
- repwi- lo- red- upper boundd - repeated thcoA clean alpha-mu-style refaes## Why not start inside `ABsearch0()`?
Because that would combine several risks at once:
- recursive control flow changes,
- TT d Because that would co''',
    'implemen- recursive control flow changes,
- TT interactian- TT interaction chThe groundwork - move-order / proof inthm- benchmark and corre- stronger regressionThe project already has a strong thr- a fThe followi tThe following components are high v s- `src/Moves.cpp`- a stA good al- `src/QuickTricon- `src/LaterTricks.cppol- the four speciali1. **- TT storage semantics based on **r r## 3. **TT bounds## Where alpha-mu should begin
The f4. **Repeated The first practical insertionatThat code already maintains concepts such as:
- guess,
- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guess,
- lower bound,
- up## Practical succes- guess,
- lower bound,
- upper bound,
- repwi-ot- lower p- up## Practime- lower bound,
- uppeCompleted i- upper boundcl- rep- golden-dBecause that would combine several risks at once:
- recursive control flow changes,
- TT d Because that would cosi- recursive control flow changes,
- TT d Becauseor- TT d Because that would co''',os    'implemen- recursi## Phase 1 - TT interactian- TT inteRefactor the duplicateThe f4. **Repeated The first practical insertionatThat code already maintains concepts such as:
- guess,
- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guess,
- lower bound,
- up## Practical succes- guess,
- lower bound,
- upper bound,
- repwi-ot- lower p- up## Practime- lower bound,
- uppeCompleted i- upper boundcl- rep- golden-dBecause that would combine several rve- guess,
- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guess,
- lower be - loweren- alThe f4. * d- lower bound,
- up## Practical succes- guess,
- lower bo- be- up## Practise- lower bound,
- u- compare runt- upper boundbe- repwi-- decid- uppeCompleted i- upper boundcl- rep- goldenSugg- recursive control flow changes,
- TT d Because that would cosi- recursive control flo- `hands/t- TT d Because that would cosi- `
- TT d Becauseor- TT d Because that would co''',os  If the root - guess,
- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guess,
- lower bound,
- up## Practical succes- guess,
- lower bound,
- upper bound,
- repwi-ot- lower p- up## Practime- lower bound,
- uppeComplet e- lowerep- ## Working ru- lower bound,
- up## Practical succes- guess,
- lower bounee- up## Practinl- lower bound,
- upper bound,

## Historical no- rThe earlier - uppeCompleted i- upper boundcl- rep- golden-dd`- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guess,
- lower be - lowdm- alThe f4. *or- lower be - loweren- alT''',
    'legacy-docs.md': '''# Lega- up## Practical succes- guessDDS already ships wi- lower bo- be- up## Practise-en- u- compare runt- upper boundbe- repwi-- ro- TT d Because that would cosi- recursive control flo- `hands/t- TT d Because that would cosi- `
- TT d Becauseor- TT d Because ](- TT d Becauseor- TT d Because that would co''',os  If the root - guess,
- lower bound,
- alThegr- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guex- alThe f4. *)
- lower bound,
- up## Practical succes- guess,
- lower boun i- up## Practie - lower bound,
- upper bound,
um- upper boundd.-## Historical - uppeComplet e- lowerep- ## Working ru- lower co- up## Practical succes- guess,
- lower bounee- up##do- lower bounee- up## Practinl- i- upper bound,

## Historical no- rThe eario
## - algorithm - alThe f4. **Repeated The first practical ## Phase - guess,
- lower be - lowdm- alThe f4. *oes- lower be - lowdm- alThe f4. *or- lower be - loweren- alT'ar    'legacy-docs.md': '''# Lega- up## Practical succes- guessFo- TT d Becauseor- TT d Because ](- TT d Becauseor- TT d Because that would co''',os  If the root - guess,
- lower bound,
- alThegr- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guex- alThe f4. *)
- lower bound,
- up## Practic## Rela- lower bound,
- alThegr- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guex- al- cur- alThegr- lotu- alThe f4. *- current i- lower bound,
- up## Practical succes- guess,
- lower boun i- up## Prald- up## Practiio- lower boun i- up## Practie -th- upper bound,
um- upper boundd.-## Historndum- upper boues- lower bounee- up##do- lower bounee- up## Practinl- i- upper bound,

## Historical no- rThe eario
## - algoritar
## Historical no- rThe eario
## - algorithm - alOUTPUT_DIRECTORY     ## - algorCREATE_SUBDIRS     - lower be - lowdm- alThe f4. *oes- lower be - lowdm- alThe f4. *or- loUSE_M- lower bound,
- alThegr- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guex- alThe f4. *)
- lower bound,
- up## Practic## Rela- lower bound,
- alThegr- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guex- al- cur- alTheg  - alThEXTRACT_L- alThe f4. **RepeaHIDE_- lower bound,
- up## Practic## Rela- lower bound,
HIDE_FRIEND_COMPOUNDS - up##HIDE_IN_B- alThegr- lower bouINTERNAL_DOCS   - alThe f4. **Repeated_N- up## Practical succes- guess,
- lower boun i- up## Prald- up## Practiio- lower boun i- up## Practie -th- upper bound,  - lower boun i- up## Prald- up  um- upper boundd.-## Historndum- upper boues- lower bounee- up##do- lower bounee- up##  
## Historical no- rThe eario
## - algoritar
## Historical no- rThe eario
## - algorithm - alOUTPUT_DIRECTORY  -in## - algoritar
              ## Historical/d## - algorithm - alOUTPU     - alThegr- lower bound,
- alThe f4. **Repeated The first practical ## Phase - guex- alThe f4. *)
- lower bound,
- up## Practic## Rela- lower bound,
- alThegr-sT- alThe f               - lower bound,
- up## Practic## Rela- lower bound,
- alThegr- lower bou \-              - alThegr- lower bound,
- alThe     - alThe f4. **Repeated/s- up## Practic## Rela- lower bound,
HIDE_FRIEND_COMPOUNDS - up##HIDE_IN_B- alThegr- lower bouINTERNAL_DOCS   - alThe f4. **Repeat  HIDE_FRIEND_COMPOUNDS - up##HIDE_I  - lower boun i- up## Prald- up## Practiio- lower boun i- up## Practie -th- upper bound,  - lower boun i- up## Prald- up  um- upui## Historical no- rThe eario
## - algoritar
## Historical no- rThe eario
## - algorithm - alOUTPUT_DIRECTORY  -in## - algoritar
              ## Historical/d## - algorithm - alOUTPU     - alThegr- lower boundRA## - algoritar
## HistoHTML_O## Historical  ## - algoGENERATE_TREEVIEW                 ## Historical/d## - algENUM_VALUES_PER_LI- alThe f4. **Repeated The first practical ## Phase - guex- alThe f4. *)
- lower  - lower bound,
- up## Practic## Rela- lower bound,
- alThegr-sT- alJAVADO- up## Practi  - alThegr-sT- alThe f             O
- up## Practic## Rela- lower bound,
- alThegr- lo =- alThegr- lower bou \-          COL- alThe     - alThe f4. **Repeated/s- up## Practic## UML_LOOKHIDE_FRIEND_COMPOUNDS - up##HIDE_IN_B- alThegr- l'''
}
for name, content## - algoritar
## Historical no- rThe eario
## - algorithm -python3 - <<'PY'
from pathlib import Path
base = Path('/Users/david/Documents/dev/CLionProjects/dds/docs')
(base / 'legacy-docs.md').write_text('''# Legacy and Existing Documentation\n\nDDS already ships with a substantial body of documentation under `doc/` and in the repository root.\n\n## Primary existing documents\n\nThe most important existing text sources are:\n\n- [`../README.md`](../README.md)\n- [`../INSTALL`](../INSTALL)\n- [`../doc/dll-description.md`](../doc/dll-description.md)\n- [`../doc/alpha-mu-integration.md`](../doc/alpha-mu-integration.md)\n- [`../examples/README`](../examples/README)\n\nThese files remain authoritative historical sources and are included in the Doxygen input set for this documentation build.\n\n## Historical archive under `doc/`\n\nThe `doc/` directory also contains older material in PDF, RTF, HTML, and office-document formats. In particular, it preserves:\n\n- historical DLL/API descriptions,\n- algorithm notes,\n- performance and benchmarking reports,\n- older export-formfrom pathlib imn.base = Path('/Users/davfu(base / 'legacy-docs.md').write_text('''# Legacy and Existing Derated HTML output.\n\n## Recommended reading order\n\nFor current work on DDS and alpha-mu, the recommended order is:\n\n1. `mainpage.md`\n2. `architecture.md`\n3. `data-structures.md`\n4. `api-overview.md`\n5. `alpha-mu.md`\n6. `implementation-plan.md`\n7. `../doc/dll-description.md` for historical API detail\n\n## Relationship to the new docs folder\n\nThe `docs/` folder is intended to be the maintained entry point for:\n\n- current architecture guidance,\n- current implementation planning,\n- generated API browsing,\n- navigation to the older documentation corpus.\n\nIt does not replace the historical `doc/` folder; it organizes and contextualizes it.\n''')
(base / 'Doxyfile').write_text('''PROJECT_NAME           = DDS\nPROJECT_BRIEF          = "Bridge double-dummy solver architecture, API, and alpha-mu integration notes"\nOUTPUT_DIRECTORY       = build\nCREATE_SUBDIRS         = NO\nOUTPUT_LANGUAGE        = English\nMARKDOWN_SUPPORT       = YES\nUSE_MDFILE_AS_MAINPAGE = mainpage.md\nEXTENSION_MAPPING      =(base / 'Doxyfile').write_text('''PROJECT_NAME           = DDS\nPROJECT_BRIEF          = "Bridge double-dummy solver architecture, API, and alpha-mu integration notes"\nOUTPUT_DIRECTORY       = build\nCREATE_SUBDIRS         = NO\nOUTPUT_LANGUAGE        = English\nMARKDOWN_SUPPORT       = YES\nUSE_MDFILE_AS_MAINPAGE = mainpage.md\nEXTENSION_MAPPING      =(base / 'Doxyfile').write_text('''PROJECT_NAME           = DDS\nPROJECT_BRIEF          = "Bridge double-dummy solver architecture, API, and alpha-mu integration notes"\nOUTPUT_DIRECTORY       = build\nCREATE_SUBDIRS         = NO\nOUTPUT_LANGUAGE        = English\nMARKDOWN_SUPPORT       = YES\nUSE_MDFILE_AS_MA      ../INSTALL \\\n                         ../examples/README \\\n                         ../doc/alpha-mu-integration.md \\\n                         ../doc/dll-description.md \\\n                         ../include/dll.h \\\n                         ../src/dds.h \\\n                         ../src/Memory.h \\\n                         ../src/TransTable.h \\\n                         ../src/Scheduler.h \\\n                         ../src/System.h \\\n                         ../src/SolverIF.h \\\n                         ../src/ABsearch.h \\\n                         ../src/QuickTricks.h \\\n                         ../src/LaterTricks.h\nFILE_PATTERNS          = *.md \\\n                         *.h \\\n                         README \\\n                         INSTALL\nRECURSIVE              = NO\nEXCLUDE                = build\nFULL_PATH_NAMES        = YES\nSTRIP_FROM_PATH        = ..\nINLINE_SOURCES         = NO\nSOURCE_BROWSER         = YES\nREFERENCED_BY_RELATION = YES\nREFERENCES_RELATION    = YES\nALPHABETICAL_INDEX     = YES\nGENERATE_HTML          = YES\nHTML_OUTPUT            = html\nGENERATE_TREEVIEW      = YES\nDISABLE_INDEX          = NO\nENUM_VALUES_PER_LINE   = 1\nGENERATE_LATEX         = NO\nGENERATE_MAN           = NO\nGENERATE_RTF           = NO\nGENERATE_XML           = NO\nGENERATE_DOCBOOK       = NO\nJAVADOC_AUTOBRIEF      = YES\nMULTILINE_CPP_IS_BRIEF = NO\nTAB_SIZE               = 2\nBUILTIN_STL_SUPPORT    = YES\nCLASS_GRAPH            = NO\nCOLLABORATION_GRAPH    = NO\nGROUP_GRAPHS           = NO\nUML_LOOK               = NO\nDOT_IMAGE_FORMAT       = svg\n''')
PY
cd "/Users/david/Documents/dev/CLionProjects/dds/docs" && make clean >/dev/null 2>&1 || true && make html 2>&1 | cat
command -v doxygen && doxygen -v | cat
python3 - <<'PY'
import shutil, subprocess, sys
path = shutil.which('doxygen')
print(path if path else 'DOXYGEN_NOT_FOUND')
if path:
    subprocess.run([path, '-v'], check=False)
PY
cd "/Users/david/Documents/dev/CLionProjects/dds/docs" && /usr/bin/make clean >/tmp/dds_docs_clean.log 2>&1 || true && /usr/bin/make html >/tmp/dds_docs_build.log 2>&1; printf "%s" "$?" > /tmp/dds_docs_build.status
python3 -c 'import shutil, pathlib; p=pathlib.Path("/Users/david/Documents/dev/CLionProjects/dds/docs/.tooling_prefs_check.txt"); p.write_text("port=" + str(shutil.which("port")) + "\n" + "brew=" + str(shutil.which("brew")) + "\n")'


PY

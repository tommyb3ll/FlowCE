<h1 align="center">FlowCE</h1>

<p align="center">
  <b>Calculus on your TI-84 Plus CE.</b><br>
  Type math the way it looks. Get answers the way your textbook writes them, and now the steps.
</p>

<p align="center">
  <a href="https://tommyb3ll.github.io/FlowCE/"><img src="https://img.shields.io/badge/install-from_the_web-245ccc" alt="Install from the web"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="License: GPL-3.0"></a>
  <img src="https://img.shields.io/badge/calculator-TI--84%20Plus%20CE-2a5bd7" alt="TI-84 Plus CE">
  <img src="https://img.shields.io/badge/engine-Giac-555" alt="Engine: Giac">
</p>

<p align="center">
  <img src="docs/flowce_light.gif" width="48%" alt="FlowCE in the Paper theme: a definite integral, a series, other forms and a graph">
  <img src="docs/flowce_steps.gif" width="48%" alt="FlowCE Steps: an antiderivative, then enter for its steps: integration by parts, down to the answer">
</p>

FlowCE is a computer algebra system for the TI-84 Plus CE, built for Calc 1 and Calc 2. It gives
exact answers: `π²/6`, not `1.64493`; `ln|sec x + tan x| + C`; `(2n²+3n+1)/(6n²)`. The decimal value
sits right under the exact one. The interface is new from top to bottom: a clean light or dark
look, math drawn as you type it, and keys that work the way a TI user expects.

**[Install it from the FlowCE page](https://tommyb3ll.github.io/FlowCE/)**: plug in the calculator,
pick a version, click Install, then press `enter` once on the calculator.

## Two versions

| | **FlowCE** | **FlowCE Steps** (beta) |
|---|---|---|
| Version | 1.9, stable | 2.0 beta 1 |
| Step-by-step solutions | | derivatives, integrals, limits, sums, Taylor series |
| Programs in Python syntax | yes | no (no room left for it) |
| Download | [`FlowCE.b84`](https://github.com/tommyb3ll/FlowCE/releases/latest/download/FlowCE.b84) | [`FlowCE-steps.b84`](https://github.com/tommyb3ll/FlowCE/releases/download/v2.0-beta1/FlowCE-steps.b84) |

Both have everything else below. They don't fit on the calculator together: install one.

## Steps (beta)

After a derivative, an integral, a limit, a sum or a Taylor polynomial, press `enter`: FlowCE
shows how to get the answer, the way a worked example in a textbook does. Each step says what it
does and the rule it uses (integration by parts, the chain rule, L'Hôpital's rule, partial
fractions, the ratio test...); a part worked out on the side, like the integral left after
integrating by parts, gets its own steps. `▲` and `▼` move from step to step.

| | | |
|:-:|:-:|:-:|
| ![Steps for the integral of x sin x: integration by parts](docs/screenshots/steps_parts.png) | ![Steps for a limit: L'Hôpital's rule](docs/screenshots/steps_limit.png) | ![Steps for a telescoping sum: partial fractions](docs/screenshots/steps_sum.png) |
| integration by parts | a limit, L'Hôpital's rule | a sum, partial fractions |
| ![Steps for a derivative: the product rule](docs/screenshots/steps_deriv.png) | ![Steps for a Taylor polynomial](docs/screenshots/steps_taylor.png) | ![The answer with the steps hint](docs/screenshots/steps_hint.png) |
| a derivative, the product rule | a Taylor polynomial | `enter` after an answer |

Not every problem has steps yet: when no textbook method explains the answer, it says so. It's a
beta: if a step looks wrong, please say what you typed on the
[Cemetech thread](https://www.cemetech.net/forum/viewtopic.php?t=21437).

## Highlights

- **Type math as it looks.** Fractions, powers, roots, integrals, sums, limits and derivatives are
  drawn in 2D while you type. `/` makes a fraction of what is before it, `^` opens an exponent,
  and the arrows move through the boxes in the order you read them. Commands from the menus and
  the search come with a box for each argument, labeled with what goes in it, and a line under
  the input says what the command does, with an example.
- **Answers like your textbook.** `arctan((x+1)/2)`, `x − x³/6 + x⁵/120 + O(x⁶)`,
  `C1·cos(x) + C2·sin(x)`, polynomials in descending powers, `log` meaning base 10, `+ C` on
  antiderivatives. Press **F4** for other forms: simplified, one fraction, factored, expanded,
  decimal. To keep answers exactly as computed, turn off *Simplify answers* in `mode`.
- **Calculus 1, 2 and 3.** Derivatives, integrals (trig integrals, trig substitution, partial
  fractions, arc length and surface area), improper integrals that say *diverges* when they do,
  limits that say *does not exist* (with the left and right values), series and power series
  sums, Taylor series, Riemann sums, differential equations with initial values, parametric and
  polar areas, gradients, multiple integrals, complex numbers, vectors, matrices.
- **Keys that work like a TI's.** `(-)` is a minus sign and `2nd (-)` is **Ans**. Starting a line
  with `×`, `÷`, `+`, `−` or `^` works on the last answer (`× 2 enter` doubles it). `2nd enter`
  brings back your last input, and `sto→` stores (`5 → b`).
- **Graphs.** Graph the line you typed, or the last answer, in one step. Trace along the curve,
  zoom, find roots and extrema, and see a table of values.
- **History and search.** **▲** walks through your past calculations; enter reuses an input or an
  answer. **▼** searches every command, with descriptions and examples.
- **Paper and Night themes**, switched from the *more* menu.
- **Tested on the calculator itself.** Before a release, about 900 Calc 1–3 problems run on an
  emulated TI-84 Plus CE and their answers are checked against a computer's. A long calculation
  shows *Calculating*, and **ON** stops it; one that runs out of memory stops with a message
  instead of closing the app.

## Screenshots

| | | |
|:-:|:-:|:-:|
| ![An antiderivative with + C](docs/screenshots/hero_integral.png) | ![Typing a definite integral](docs/screenshots/typing_integral.png) | ![The sum of 1/n² is π²/6](docs/screenshots/basel.png) |
| an antiderivative, with `+ C` | typing a definite integral | Σ 1/n² = π²/6 |
| ![A power series sum](docs/screenshots/power_series.png) | ![A limit that does not exist](docs/screenshots/dne.png) | ![A Riemann sum](docs/screenshots/riemann.png) |
| a power series: −ln(1−x) | a limit that does not exist | a Riemann sum, index i |
| ![The integral of sec cubed](docs/screenshots/sec.png) | ![A trig substitution](docs/screenshots/trigsub.png) | ![An initial value problem](docs/screenshots/ivp.png) |
| ∫ sec³x | a trig substitution | an initial value problem |
| ![F4: other forms](docs/screenshots/forms.png) | ![A graph with trace](docs/screenshots/graph_sin_trace.png) | ![Table of values](docs/screenshots/table.png) |
| F4: other forms | trace along a graph | a table of values |
| ![History](docs/screenshots/history.png) | ![Command search](docs/screenshots/search.png) | ![Night theme](docs/screenshots/night.png) |
| history | command search | Night theme |

## Install

**The easy way: [the FlowCE page](https://tommyb3ll.github.io/FlowCE/#install).** Open it in Chrome
or Edge with the calculator plugged in, pick FlowCE or FlowCE Steps, and click Install: it checks
the calculator, erases its archive (after you agree), sends FlowCE and opens the installer. Press
`enter` once on the calculator: it installs and restarts by itself. On Windows it needs
[TI Connect CE](https://education.ti.com/en/products/computer-software/ti-connect-ce-sw) 6.1 or
newer installed (for its USB driver).

You need a **TI-84 Plus CE** (or TI-83 Premium CE) on **OS 5.3 to 5.8.4**. FlowCE fills most of the
memory (about 2.8 MB), so installing it erases the calculator's apps and archived files: back up
anything you want to keep first. FlowCE is a computer algebra system: follow your class and test
rules (some tests, like the ACT, don't allow one).

**By hand, with TI Connect CE:**

1. **Clear the memory:** `2nd` `+` (MEM) → `7:Reset...` → right arrow twice to **ALL** →
   `1:All Memory...` → `2:Reset`.
2. **Send the bundle:** download [`FlowCE.b84`](https://github.com/tommyb3ll/FlowCE/releases/latest/download/FlowCE.b84)
   or [`FlowCE-steps.b84`](https://github.com/tommyb3ll/FlowCE/releases/download/v2.0-beta1/FlowCE-steps.b84),
   drag it onto the calculator in TI Connect CE and send it.
3. **Run the installer:** `prgm` → `enter` → `enter` opens the arTIfiCE shell; `enter` starts
   INST and `enter` again installs. It counts down to 0; don't touch the calculator.
4. At *"Success! Will now reset"*, press `enter` (a few times if the screen looks odd).
5. **Start:** `clear`, then `apps` → **FlowCE**.

| 1. Clear the memory | 3. Run the installer | 5. Start FlowCE |
|:-:|:-:|:-:|
| ![Clearing the memory: MEM, Reset, ALL, All Memory, Reset](docs/install/install_1_reset.gif) | ![prgm, enter, enter, the arTIfiCE shell, INST, the countdown, Success](docs/install/install_3_run.gif) | ![apps, FlowCE, the start screen](docs/install/install_5_start.gif) |

**If something goes wrong**
- *The screen freezes at "Success! Will now reset":* press the reset button on the back of the
  calculator (a pen tip or a paperclip). FlowCE stays installed.
- *The calculator acts strangely* (menus glitch, TI Connect CE stays on "refreshing"): clear the
  RAM: `2nd` `+` (MEM) → `7:Reset...` → `1:All RAM...` → `2:Reset`. This does not remove FlowCE.
- *The calculator stays unresponsive:* hold the reset button for a few seconds.

## Keys

| Key | What it does |
|---|---|
| F1 to F5 | menus: algebra, calculus, trig, symbols, more (with `2nd` or `alpha`: more menus) |
| F1 / F2 action after typing | applies to what you typed: `x^2−1` then Factor is `factor(x^2−1)` (on an empty line: the last answer) |
| `enter` after a calculus answer | its steps (FlowCE Steps) |
| `math` | templates: fraction, root, integrals, derivative, limit, sum |
| ▲ / ▼ | history / search every command |
| F4 on an answer | its other forms |
| `(-)` | a minus sign; `2nd (-)` is the last answer (Ans) |
| `+` `−` `×` `÷` `^` `x²` first | works on the last answer, as on a TI (`× 2` is Ans·2) |
| `2nd enter` | the last input again (ENTRY); press again for the one before |
| `sto→` | stores a value: `5 → b` |
| `clear` | erases the line; on an empty line, the answer |
| ON or `clear` while computing | stops the calculation |
| *more* → Graph | graphs the line or the answer; in the graph: F2 window and curve study, `2nd graph` table |
| `mode` | settings (*Simplify answers* on or off), shortcuts, about |
| `2nd quit` | quit (your session is kept) |

## Building

FlowCE builds with the [CE C/C++ toolchain](https://github.com/CE-Programming/toolchain) (CEdev
v14.2) plus `g++` and `python3`. Clone with `--recursive` (the engine is the `src/giac`
submodule). `./mkappen` builds the English app: AppIns appvars (`.8xv`) installed by the INST
program, and `FlowCE.b84` for TI Connect CE. After `make clean`, `STEPS=1 ./mkappen` builds FlowCE
Steps (step-by-step solutions instead of Python syntax), and `UNITS=1 ./mkappen` builds physical
units instead of Python syntax. The step-by-step code is on the `dev/ux-speed` branch; `main` is
the 1.9 release. `tools/dev/build.sh` builds in WSL from the working tree, `tools/emu/` drives the
CEmu emulator for regression tests, screenshots and the GIFs above, and `tests/host/` holds host
tests of the editor and the 2D layout.

The app must fit in 43 flash pages (at most 2,818,045 bytes); the build prints the room left.

## Credits and license

FlowCE is a fork of [KhiCAS](https://www-fourier.univ-grenoble-alpes.fr/~parisse/) by Bernard
Parisse.

- **Math engine:** Giac, by Bernard Parisse and contributors (Institut Fourier, Université
  Grenoble Alpes).
- **Console interface:** adapted from Eigenmath for the Casio Prizm by G. Maia, Mike Smith,
  Nemhardy and LePhenixNoir.
- **Thanks** to Adrien Bertrand, Xavier Andreani and the TI-83/84 development community,
  especially Jacob Young, commandblockguy and Matt Waltz.
- **Installer:** [arTIfiCE](https://yvantt.github.io/arTIfiCE/) by YvanTT.
- **Fonts:** Atkinson Hyperlegible (Braille Institute) and STIX Two, under the SIL Open Font
  License (`tools/fonts/src`).

FlowCE is free software under the [GNU General Public License](LICENSE), version 3 or later. It
comes with no warranty.

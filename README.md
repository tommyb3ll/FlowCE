<h1 align="center">FlowCE</h1>

<p align="center">
  <b>Calculus on your TI-84 Plus CE.</b><br>
  Type math the way it looks. Get answers the way your textbook writes them, and every step to get there.
</p>

<p align="center">
  <a href="https://tommyb3ll.github.io/FlowCE/"><img src="https://img.shields.io/badge/install-from_the_web-245ccc" alt="Install from the web"></a>
  <a href="https://github.com/tommyb3ll/FlowCE/releases/latest"><img src="https://img.shields.io/github/v/release/tommyb3ll/FlowCE?label=release&color=245ccc" alt="Latest release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="License: GPL-3.0"></a>
  <img src="https://img.shields.io/badge/calculator-TI--84%20Plus%20CE-2a5bd7" alt="TI-84 Plus CE">
  <img src="https://img.shields.io/badge/engine-Giac-555" alt="Engine: Giac">
</p>

<p align="center">
  <img src="docs/flowce_light.gif" width="48%" alt="FlowCE: the start screen, a definite integral typed in 2D, the sum of 1/n² giving π²/6, other forms of an answer, and a graph traced">
  <img src="docs/flowce_steps.gif" width="48%" alt="FlowCE: an antiderivative from the calculus menu, then enter for its steps: integration by parts, down to the answer">
</p>

FlowCE is a computer algebra system for the TI-84 Plus CE, built for Calc 1 and Calc 2. It gives
exact answers: `π²/6`, not `1.64493`; `ln|sec x + tan x| + C`; `(2n²+3n+1)/(6n²)`. The decimal value
sits right under the exact one. Press `enter` after an answer and FlowCE shows you how to get it,
step by step, the way a worked example in your textbook does.

**[Install it from the FlowCE page](https://tommyb3ll.github.io/FlowCE/)**: plug in the calculator,
pick a version, click Install, and follow the short calculator prompts.

## What's new

- **2.2:** themes you can tell apart on the calculator's screen, their colors picked on a TI-84 Plus CE (2.2.1: a clean start screen).
- **2.1:** faster: menus open in about half the time, everyday problems answer 25 to 45% sooner.
- **2.0:** steps for algebra, Calculus → Applications, a real graph screen, nine themes, and more:

<table>
  <tr>
    <td width="33%"><img src="docs/flowce_themes.gif" alt="FlowCE in use while its theme changes: Paper, Night, Sand, Mint, Arctic, Ember, Rose, Grape, Forest"></td>
    <td width="33%"><img src="docs/flowce_apps.gif" alt="Calculus, Applications, Area between curves: the form filled in, the area, its steps"></td>
    <td width="33%"><img src="docs/flowce_graph.gif" alt="The graph screen: x³ − 3x typed, graphed, panned, its key points labeled, traced"></td>
  </tr>
  <tr>
    <td align="center"><b>Nine themes</b></td>
    <td align="center"><b>Calculus → Applications</b></td>
    <td align="center"><b>A new graph screen</b></td>
  </tr>
</table>

- **Steps for algebra too.** On top of derivatives, integrals, limits, sums and Taylor series:
  linear and quadratic equations, factoring, rational and radical equations, systems,
  exponential, log and trig equations, inequalities, absolute values and polynomial division.
- **Applications of calculus**, filled in like a worksheet. Area between curves, volumes by washers
  or shells, arc length, surface area and average value each open a short form (*between y =,
  and y =, from x =, to x =*). Leave the bounds empty and FlowCE finds where the curves meet; then
  `enter` shows the whole setup and the integral, step by step.
- **A real graph screen.** Arrows pan and `+` `−` zoom; trace along the curve; the key points
  (zeros, extrema, inflections) marked and labeled, exact when you jump to one; a table; tangent
  lines, areas, max and min on an interval. Equations in x and y (circles, ellipses), parametric
  and polar curves, slope fields.
- **Nine themes.** Paper, Night, Sand, Mint, Arctic, Ember, Rose, Grape and Forest. *more* →
  Themes shows each one on the whole screen as you move to it.
- **`analyze(f)`**: domain, intercepts, asymptotes, increasing and decreasing, extrema,
  concavity and inflection points on one page.
- **Statistics and probability** in the *more* menu: enter data, box plots, fitted curves with r²;
  normal, t, chi-square, binomial and Poisson distributions, inverses, intervals and tests.
- **What next?** Under an answer, `▶` offers the likely next step: factor it, solve it for 0,
  graph it, take its derivative.
- **Answers that read better.** Matrices drawn as matrices, long lists of solutions wrapped,
  `F4` with long division and the completed square, an antiderivative's derivative as a check,
  `y′` in the symbols menu for `y″ + 4y = 0` and `f′(2)`.

## Two versions

| | **FlowCE** 2.2.1 | **FlowCE Python** 1.9.1 |
|---|---|---|
| Step-by-step solutions | calculus (derivatives, integrals, limits, sums, Taylor series) and algebra (equations, factoring, systems, inequalities) | |
| Graphs | the graph screen: key points, pan and zoom, conics, parametric, polar, slope fields | the classic graph, trace, table |
| Calculus → Applications | area, volumes, arc length, surface area, average value | |
| Statistics and probability | data, box plots, regressions, distributions, tests | |
| Themes | nine | Paper and Night |
| Programs (Python syntax, the program editor) | | yes |
| Download | [`FlowCE.b84`](https://tommyb3ll.github.io/FlowCE/files/FlowCE.b84) | [`FlowCE-python.b84`](https://tommyb3ll.github.io/FlowCE/files/FlowCE-python.b84) |

FlowCE is the one for class; FlowCE Python is for writing programs. Both have everything under
[Highlights](#highlights). Install one at a time.

## Steps

After an answer, press `enter`. Each step says what it does and the rule it uses (integration by
parts, the chain rule, L'Hôpital's rule, the quadratic formula, elimination...); a part worked
out on the side gets its own steps. `▲` and `▼` move from step to step, and `F4` shows another
method when there is one.

| | | |
|:-:|:-:|:-:|
| ![Steps for the integral of x sin x: integration by parts](docs/screenshots/steps_parts.png) | ![Steps for a quadratic equation: the quadratic formula](docs/screenshots/steps_quadratic.png) | ![Steps for a limit: L'Hôpital's rule](docs/screenshots/steps_limit.png) |
| integration by parts | a quadratic, by the formula | a limit, L'Hôpital's rule |
| ![Steps for a system of equations: elimination](docs/screenshots/steps_system.png) | ![Steps for a log equation](docs/screenshots/steps_log.png) | ![Steps for an inequality: a number tested in each interval](docs/screenshots/steps_ineq.png) |
| a system, by elimination | a log equation | an inequality |
| ![Steps for a telescoping sum: partial fractions](docs/screenshots/steps_sum.png) | ![Steps for an absolute value equation](docs/screenshots/steps_abs.png) | ![Steps for a derivative: the chain rule](docs/screenshots/steps_deriv.png) |
| a sum, partial fractions | an absolute value | a derivative |

Not every problem has steps yet: when no textbook method explains the answer, FlowCE says so. If a
step looks wrong, please [open an issue](https://github.com/tommyb3ll/FlowCE/issues/new/choose)
with what you typed, or post on the [Cemetech thread](https://www.cemetech.net/forum/viewtopic.php?t=21437).

## Applications, graphs, statistics

| | | |
|:-:|:-:|:-:|
| ![The form of Area between curves: between y = x + 2 and y = x², from and to where they meet](docs/screenshots/apps_form.png) | ![Area between curves, with steps](docs/screenshots/apps_area_steps.png) | ![Volume by washers, with steps](docs/screenshots/apps_washer_steps.png) |
| Calculus → Applications: a form | find where they meet, then the area | a volume, by washers |
| ![The graph screen: key points of x³ − 3x](docs/screenshots/graph_points.png) | ![An ellipse and a line on the graph screen](docs/screenshots/graph_conic.png) | ![A slope field](docs/screenshots/graph_field.png) |
| the graph screen: key points | equations in x and y | a slope field |
| ![analyze: domain, intercepts, asymptotes](docs/screenshots/analyze.png) | ![A line fitted to data](docs/screenshots/stats_fit.png) | ![The normal distribution: an area](docs/screenshots/prob_normal.png) |
| `analyze(f)` | statistics: a fitted line | probability: the normal curve |
| ![The row of actions under an answer](docs/screenshots/action_row.png) | ![A box plot and the five numbers](docs/screenshots/stats_box.png) | ![A t test](docs/screenshots/prob_ttest.png) |
| `▶`: what to do next | a box plot | a t test |

## Themes

<p align="center">
  <img src="docs/screenshots/themes_grid.png" width="60%" alt="more, Themes: nine tiles, Paper, Night, Sand, Mint, Arctic, Ember, Rose, Grape and Forest, each in its own colors">
</p>

*more* → **Themes** opens a grid of all nine, each tile in its own colors. Moving to a tile shows
that theme on the whole screen at once; `enter` keeps it, `clear` goes back. Paper and Night are
FlowCE's light and dark looks; the others take after well-loved palettes (Solarized, Nord,
Gruvbox, Rosé Pine, Dracula).

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
  brings back your last input, and `sto→` stores (`5 → b`). Keys typed while a menu opens are
  kept, however fast you type.
- **History and search.** **▲** walks through your past calculations; enter reuses an input or an
  answer. **▼** searches every command, with descriptions and examples.
- **Tested on the calculator itself.** Before a release, about 900 Calc 1–3 problems run on an
  emulated TI-84 Plus CE and their answers are checked against a computer's, along with every
  menu and screen. A long calculation shows *Calculating*, and **ON** stops it; one that runs out
  of memory stops with a message instead of closing the app.

## More screens

| | | |
|:-:|:-:|:-:|
| ![An antiderivative with + C](docs/screenshots/hero_integral.png) | ![Typing a definite integral](docs/screenshots/typing_integral.png) | ![The sum of 1/n² is π²/6](docs/screenshots/basel.png) |
| an antiderivative, with `+ C` | typing a definite integral | Σ 1/n² = π²/6 |
| ![A power series sum](docs/screenshots/power_series.png) | ![A limit that does not exist](docs/screenshots/dne.png) | ![A Riemann sum](docs/screenshots/riemann.png) |
| a power series: −ln(1−x) | a limit that does not exist | a Riemann sum, index i |
| ![The integral of sec cubed](docs/screenshots/sec.png) | ![A trig substitution](docs/screenshots/trigsub.png) | ![An initial value problem](docs/screenshots/ivp.png) |
| ∫ sec³x | a trig substitution | an initial value problem |
| ![F4: other forms](docs/screenshots/forms.png) | ![A graph with trace](docs/screenshots/graph_sin_trace.png) | ![A polar curve](docs/screenshots/graph_polar.png) |
| F4: other forms | trace along a graph | a polar curve |
| ![History](docs/screenshots/history.png) | ![Command search](docs/screenshots/search.png) | ![The calculus menu](docs/screenshots/menu_calculus.png) |
| history | command search | the calculus menu |

## Install

**[The FlowCE page](https://tommyb3ll.github.io/FlowCE/#install) selects the installer for your OS.** Open it in Chrome
or Edge with the calculator plugged in, pick FlowCE or FlowCE Python, and click Install: it checks
the calculator, erases its archive (after you agree), sends FlowCE and opens the installer. Press
`enter` once on the calculator: it installs and restarts by itself. On Windows it needs
[TI Connect CE](https://education.ti.com/en/products/computer-software/ti-connect-ce-sw) 6.1 or
newer installed (for its USB driver).

**OS 5.8.5:** the page selects **arTIfiCE v3** and guides you through Inequalz, **AsmHook2**,
then **INST**. The page supplies and transfers all required files; there is nothing to download
or select yourself. It prepares RAM and starts INST automatically, with two short calculator prompts in between.
[TI Connect CE instructions](docs/install/os-5.8.5.md) are also available.
OS **5.8.4.0058** still uses v2.1. The one-enter automatic finish above applies to the v2.1 path.
Keep your current OS; upgrading is not necessary to use FlowCE.

You need a **TI-84 Plus CE** (or TI-83 Premium CE). FlowCE fills most of the
memory (about 2.8 MB), so installing it erases the calculator's apps and archived files: back up
anything you want to keep first. FlowCE is a computer algebra system: follow your class and test
rules (some tests, like the ACT, don't allow one).

**OS 5.3–5.8.4 by hand, with TI Connect CE:**

1. **Clear the memory:** `2nd` `+` (MEM) → `7:Reset...` → right arrow twice to **ALL** →
   `1:All Memory...` → `2:Reset`.
2. **Send the bundle:** download [`FlowCE.b84`](https://tommyb3ll.github.io/FlowCE/files/FlowCE.b84)
   or [`FlowCE-python.b84`](https://tommyb3ll.github.io/FlowCE/files/FlowCE-python.b84),
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
| `enter` after an answer | its steps (calculus, equations, inequalities, factoring, divisions) |
| `▶` after an answer | the row of next steps: `◀` `▶` move, `enter` does it |
| F4 on the steps page | another method, when there is one (long or synthetic division) |
| `math` | templates: fraction, root, integrals, derivative, limit, sum |
| ▲ / ▼ | history / search every command |
| F4 on an answer | its other forms (on an antiderivative, its derivative as a check) |
| `(-)` | a minus sign; `2nd (-)` is the last answer (Ans) |
| `+` `−` `×` `÷` `^` `x²` first | works on the last answer, as on a TI (`× 2` is Ans·2) |
| `2nd enter` | the last input again (ENTRY); press again for the one before |
| `sto→` | stores a value: `5 → b` |
| `clear` | erases the line; on an empty line, the answer |
| ON or `clear` while computing | stops the calculation |
| calculus menu, `0` | Applications: area, volumes, arc length, surface area, average value (FlowCE 2.0) |
| *more* → Graph | graphs the line or the answer (FlowCE 2.0: arrows pan, `+` `−` zoom, F1 trace, F2 zoom menu, F3 key points and tools, F4 table) |
| *more* → Themes | nine themes, shown as you move (FlowCE 2.0; FlowCE Python: Paper or Night) |
| *more* → Statistics, Probability | data and fitted curves, distributions and tests (FlowCE 2.0) |
| `mode` | settings (*Simplify answers* on or off), shortcuts, about |
| `2nd quit` | quit (your session is kept) |

## Building

FlowCE 2.2.1 builds with the [CE C/C++ toolchain](https://github.com/CE-Programming/toolchain) v15
plus `g++` and `python3`. Clone with `--recursive` (the engine is the `src/giac` submodule).
`tools/dev/build.sh en-steps-v15` builds it in WSL from the working tree (`APPLANG=en STEPS=1`;
`tools/dev/wsl_build_v15.sh` links the app and cuts it into the AppIns appvars installed by the
INST program). FlowCE Python 1.9.1 is the `v1.9.1-rc1` tag, built with CEdev v14.2 and
`./mkappen`. `tools/emu/` drives the CEmu emulator for the regression tests, the screenshots and
the GIFs above, and `tests/host/` holds host tests of the editor and the 2D layout.

The app must fit in 43 flash pages (at most 2,818,045 bytes); the build prints the room left.

## Credits and license

FlowCE, by Tommy Bell, is a fork of [KhiCAS](https://www-fourier.univ-grenoble-alpes.fr/~parisse/)
by Bernard Parisse.

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

Parts of FlowCE 2.0 (the algebra steps, the graph screen, analyze, statistics and probability)
were adapted from [DeltaCE](https://github.com/bupfff/DeltaCE), a GPL-3.0 fork of FlowCE.

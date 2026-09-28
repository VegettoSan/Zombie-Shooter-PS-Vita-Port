# PS Vita -> Xbox controls: strict mode and `controls.txt`

Date: 2026-09-27

## Hardware result — strict/hybrid are not the final fix

This pass has now been tested on a real PS Vita. The configuration/parser and the
restoration of the older event path work, but the game still does **not** interpret
the digital controls as the intended Xbox layout.

Observed hardware behavior:

- both analog sticks work;
- Cross/Circle/Square/Triangle do not perform the expected controller actions;
- L can trigger flashlight and grenade together;
- R appears to trigger another gameplay action such as healing;
- only one useful D-pad direction was observed, associated with healing;
- rear-left heals;
- rear-right throws a grenade and can also display current map/level information;
- strict and hybrid did not produce a meaningful correction of the digital mapping.

A later synthetic/gameplay-touch experiment made the situation worse by leaving
the buttons effectively dead. That experiment was removed again. Commit
`5e246c97bbf1a8de4858abb4843757dd980ed271` restored the strict/hybrid code to the
known pre-experiment state, and the user confirmed that controls returned to the
same broken behavior described above.

Therefore `input_mode strict` is still a useful diagnostic representation, but it
must **not** be described as a solved controller implementation.

The strongest current clue is that MotionEvent/axis input is healthy enough for
both sticks while digital actions are wrong. The next implementation must inspect
the exact XAPK/DEX/native controller path and determine the digital event contract
actually consumed by Zombie Shooter: KeyEvent key code, scan code, source/device,
button-state bits, HAT axes, Java callbacks or an engine-specific mapping. See
`docs/CURRENT_HARDWARE_BLOCKERS_2026-09-27.md`.

## Why this pass exists

The two analog sticks were already working on real Vita, but digital buttons were
not behaving consistently. The previous FalsoNDK path emitted some logical Xbox
controls in more than one Android representation at the same time: D-pad as both
KeyEvent and HAT axes, and LT/RT as both L2/R2 KeyEvents and trigger axes.

This pass keeps all renderer, audio, music, cache, light, resolution and gameplay
performance work untouched. Only the Vita input mapping layer and its host test
were changed.

Safety backup before the change:

`backup/pre-controller-fix-20260927`

The backup points to master commit
`f7273d94cf8709e31c4be90831e51c8c46a0bb7c`.

## New behaviour

The handheld Vita now supports two event modes selected from:

`ux0:data/zombieshooter/controls.txt`

### `input_mode strict` — recommended first diagnostic representation

This presents a cleaner Xbox-style controller to Zombie Shooter:

- A/B/X/Y -> Android gamepad KeyEvents
- LB/RB -> Android gamepad KeyEvents
- START/BACK -> Android gamepad KeyEvents
- LS/RS clicks -> Android gamepad KeyEvents
- D-pad -> HAT_X/HAT_Y motion axes only
- LT/RT -> trigger motion axes only

This prevents the Vita handheld from generating the old duplicate D-pad and
L2/R2 KeyEvents for the same logical control. Hardware testing shows that removing
those duplicates alone is insufficient to make the closed game interpret the
buttons correctly.

### `input_mode hybrid` — compatibility / A-B test

This restores the previous representation:

- D-pad still has HAT_X/HAT_Y, but also emits DPAD KeyEvents
- LT/RT still have trigger axes, but also emit BUTTON_L2/BUTTON_R2 KeyEvents

Use this only as a comparison while reconstructing the actual event contract.

External DS3/DS4 controllers do **not** use the Vita physical remapping. They keep
the complete legacy FalsoNDK representation so this handheld experiment does not
silently remap an external controller.

## Default file

If `controls.txt` does not exist, the game creates it once. It is not overwritten
on later boots.

```text
# Zombie Shooter Vita control mapping
input_mode strict

cross A
circle B
square X
triangle Y

l LB
r RB
rear_left LT
rear_right RT

start START
select BACK

dpad_up DPAD_UP
dpad_down DPAD_DOWN
dpad_left DPAD_LEFT
dpad_right DPAD_RIGHT

l3 LS
r3 RS
```

The right-hand value is a logical Xbox control, not a Zombie Shooter action.
Zombie Shooter decides what that Xbox control means in gameplay.

Supported values:

`A B X Y LB RB LT RT LS RS START BACK DPAD_UP DPAD_DOWN DPAD_LEFT DPAD_RIGHT NONE`

`NONE` disables that physical input.

Names and values are case-insensitive after normalization. Empty lines and lines
starting with `#` are ignored. A partial file changes only the entries present;
all other bindings use defaults.

## Important for an existing installation

An old `controls.txt` is deliberately preserved. Therefore a Vita that already
has the file will not automatically receive the new comments/template.

For diagnostic tests, either add this line to the existing file:

```text
input_mode strict
```

or make a backup of the existing file, rename it, launch the game once and let
the new template be generated.

Always fully close and restart Zombie Shooter after editing `controls.txt`.

## Target Xbox gameplay semantics

The game itself shows the intended controller layout. Preserve these semantics
while reconstructing the Android input contract:

- left stick: move/navigation;
- right stick: shoot/aim according to the game's controller UI;
- A: select/continue;
- B: back;
- Y: buy ammo;
- LB: next weapon;
- RB: previous weapon;
- LT: use medkit;
- RT: use grenade;
- D-pad up: medkit;
- D-pad down: grenade;
- D-pad left: previous weapon;
- D-pad right: next weapon.

The controller screen did not expose a gameplay action for X. Do not invent one.

## Suggested diagnostic tests

### Test A — standard Xbox mapping

```text
input_mode strict
cross A
circle B
square X
triangle Y
l LB
r RB
rear_left LT
rear_right RT
start START
select BACK
dpad_up DPAD_UP
dpad_down DPAD_DOWN
dpad_left DPAD_LEFT
dpad_right DPAD_RIGHT
```

Check menu accept/back first, then movement, aiming/shooting, weapon switching,
medkit/grenade and D-pad actions shown by the game's controller tutorial.

### Test B — same mapping, old Android aliases

Change only:

```text
input_mode hybrid
```

Do not change any button assignments. This isolates event representation from
button layout. Hardware testing has already shown no useful correction from this
alone, so do not spend another full pass merely retesting strict versus hybrid
without changing the underlying digital event contract.

### Test C — shoulders as triggers

If a future analysis proves that LT/RT are the useful primary actions on Vita, a
mapping experiment can still be done without rebuilding:

```text
input_mode strict
l LT
r RT
rear_left LB
rear_right RB
```

Restore the four default lines to return to the standard physical mapping.

### Test D — disable rear touch temporarily

```text
rear_left NONE
rear_right NONE
```

This remains useful to prove whether an accidental rear touch participates in an
unexpected duplicate action.

## What the next Debug build should report

The next useful diagnostic build should log one compact line per digital
transition, including:

- physical Vita button/mask;
- logical Xbox control chosen by `controls.txt`;
- Android input type and action;
- device id and source;
- key code and scan code;
- repeat/meta state if consumed;
- button-state/HAT information if relevant;
- which game-facing branch consumed or rejected the event when this can be
  established without per-frame spam.

A Release build should keep only the compact startup mapping line and important
errors.

## Implementation notes

`source/utils/gamepad.c` provides a strong replacement for FalsoNDK's weak button
table. The first ten entries are the strict KeyEvent set. Six optional
D-pad/L2/R2 aliases are appended only for handheld `hybrid` mode or external
controllers. `fndk_translate_pad_buttons()` selects the active count before
FalsoNDK emits events.

The host regression covers the parser/translation layer:

- strict face/menu/stick-click buttons
- strict HAT-only D-pad
- strict axis-only LT/RT
- hybrid legacy aliases
- arbitrary remapping
- `NONE`
- rear touch remapping
- DS3/DS4 bypass of Vita remapping
- analog sticks, touchscreen identity and queue stability

Those tests prove the port emits what the test expects; they do **not** prove the
closed game expects that event representation. Real-Vita testing has now shown the
missing piece is deeper than the mapping table. The next pass must rebuild the
digital-input contract from the exact XAPK/DEX and canonical `.so`, and may use
third-party FalsoNDK/NativeActivity Vita ports as references only after confirming
a matching API/lifecycle pattern.

# PS Vita -> Xbox controls: strict mode and `controls.txt`

Date: 2026-09-27

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

### `input_mode strict` — recommended first test

This presents a cleaner Xbox-style controller to Zombie Shooter:

- A/B/X/Y -> Android gamepad KeyEvents
- LB/RB -> Android gamepad KeyEvents
- START/BACK -> Android gamepad KeyEvents
- LS/RS clicks -> Android gamepad KeyEvents
- D-pad -> HAT_X/HAT_Y motion axes only
- LT/RT -> trigger motion axes only

This prevents the Vita handheld from generating the old duplicate D-pad and
L2/R2 KeyEvents for the same logical control.

### `input_mode hybrid` — compatibility / A-B test

This restores the previous representation:

- D-pad still has HAT_X/HAT_Y, but also emits DPAD KeyEvents
- LT/RT still have trigger axes, but also emit BUTTON_L2/BUTTON_R2 KeyEvents

Use this only as a comparison if a control does not respond in strict mode.

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

For the first test, either add this line to the existing file:

```text
input_mode strict
```

or make a backup of the existing file, rename it, launch the game once and let
the new template be generated.

Always fully close and restart Zombie Shooter after editing `controls.txt`.

## Suggested tests

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
button layout. If a button works only in hybrid mode, record exactly which one.
If one physical press causes two gameplay actions in hybrid but only one in
strict, strict is the correct representation for that control.

### Test C — shoulders as triggers

If the game proves that LT/RT are the useful primary actions on Vita, test:

```text
input_mode strict
l LT
r RT
rear_left LB
rear_right RB
```

This requires no rebuild. Restore the four default lines to return to the
standard mapping.

### Test D — disable rear touch temporarily

```text
rear_left NONE
rear_right NONE
```

This is useful to prove that an accidental rear touch is or is not responsible
for an unexpected action.

## What to report from Vita

For each test, report:

- exact `input_mode`
- the complete `controls.txt`
- Cross / Circle / Square / Triangle result
- L and R result
- rear-left and rear-right result
- all four D-pad directions
- both sticks
- whether any single physical press causes two gameplay actions

The normal log contains one compact startup line similar to:

```text
[INPUT] xbox_map source=controls.txt emulation=xbox input_mode=strict ...
```

Debug builds additionally log physical masks, resulting Xbox masks and emitted
button events without adding per-frame Release spam.

## Implementation notes

`source/utils/gamepad.c` now provides a strong replacement for FalsoNDK's weak
button table. The first ten entries are the strict KeyEvent set. Six optional
D-pad/L2/R2 aliases are appended only for handheld `hybrid` mode or external
controllers. `fndk_translate_pad_buttons()` selects the active count before
FalsoNDK emits events, so no new FalsoNDK patch was required and the large audio/
asset patch from the latest Codex work remains untouched.

The host regression now loads the real `controls.txt` parser and covers:

- strict face/menu/stick-click buttons
- strict HAT-only D-pad
- strict axis-only LT/RT
- hybrid legacy aliases
- arbitrary remapping
- `NONE`
- rear touch remapping
- DS3/DS4 bypass of Vita remapping
- analog sticks, touchscreen identity and queue stability

Physical Vita validation is still required; a source/build test cannot prove the
closed game interprets every Xbox logical control exactly as expected.

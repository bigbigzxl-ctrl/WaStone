# STM32 BMDA No-Route-To-Host Triage

## Purpose

Use this skill when an STM32 bench run appears to fail during flash or preflight
and the configured BMDA / esp32jtag instrument may simply be unreachable.

This skill prevents misclassifying a bench transport outage as:

- a new firmware regression
- a bad test plan
- a mailbox/runtime failure

## Trigger

Apply this when AEL output shows patterns such as:

- `could not connect: No route to host`
- `probe_transport_unhealthy`
- `ping <instrument_ip> -> FAIL`
- both GDB and HTTPS surfaces to the same instrument fail

## Source Case

Source board:

- `stm32f103_gpio`
- pack: `packs/stm32f103c8t6_golden.json`
- instrument: `esp32jtag_stm32_golden @ 192.168.2.109`
- live validation date: 2026-03-30

What happened:

- pack selection was correct
- builds completed
- `wiring_verify` failed in preflight
- mailbox/visual tests failed in `load`
- direct HTTPS probe to the instrument also failed with `No route to host`

## Core Rule

If both of these are true:

1. build succeeds
2. the instrument's GDB and HTTPS surfaces are both unreachable

then classify the problem as `bench/resource reachability` first.

Do not spend time changing firmware or test logic until reachability is restored.

## Procedure

1. Run the intended repo-native command once.
   Example:

   ```bash
   PYTHONPATH=. python3 -m ael pack --pack packs/<pack>.json --board <board>
   ```

2. Inspect one preflight-oriented run first.
   Prefer a `wiring_verify` or other test with explicit preflight checks.

3. Confirm these three layers separately:
   - selection: correct board and instrument were chosen
   - build: firmware build completed
   - transport: GDB / HTTPS to the instrument are reachable

4. Collect direct reachability evidence:
   - `ip -4 addr`
   - `curl -k -sS https://<instrument_ip>/`
   - run preflight log

5. If preflight says `probe_transport_unhealthy` and `curl` also reports
   `No route to host`, stop debugging the suite and move to bench recovery.

## What Evidence Separates False Leads From The Real Cause

False lead:

- "new Stage 1/2 tests broke flashing"

Evidence against that:

- the board and pack resolve correctly
- representative new targets build successfully
- failure happens before DUT execution
- the same network path also blocks direct instrument HTTPS access

Real cause:

- host cannot reach the configured bench instrument

## Recommended Next Action

1. Restore reachability to the instrument IP
2. Re-run the same pack unchanged
3. Only if reachability is healthy and failure persists, debug flash strategy,
   wiring, or firmware logic

## Reusable Lesson

For STM32 BMDA / esp32jtag paths, `No route to host` at both preflight and load
is a bench-state fact, not a firmware signal.

Always prove transport health before spending time on test or firmware changes.

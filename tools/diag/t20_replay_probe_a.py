#!/usr/bin/env python3
# SPDX-License-Identifier: BUSL-1.1
"""
og-netcode-v2-input-relay T20 -- RETROACTIVE PROBE A, replayed from logs already on disk.

WHY THIS EXISTS. T20's Probe A instruments the live build to report server sim ticks
per game-thread frame. A background agent cannot drive PIE, so before writing the
finding this script recovers the SAME quantity from the artefacts of the sessions
that produced the hit-rate numbers in RelayDepthCoverageHypothesis.md -- and, more
importantly, time-aligns it against the client-side arrival gap those sessions
measured. If the two disagree, the depth hypothesis is not the whole story and the
live run has a specific thing to look for.

HOW THE SERVER TICK IS RECOVERED, and the caveats, stated up front:
  * `[SendCorrectionStateToClients] id=<id> tick=<T>` is emitted once per character
    from SimulationNetSync::sendCorrectionAll, which SimulationManager::
    onPostSimulationGameThread calls from ASimulationManagerUImpl::OnPostPhysicsStep
    -- i.e. the GAME thread, on the exact hook T20's dispatch named as the candidate.
  * UE's log line prefix carries `GFrameCounter % 1000` in its second bracket, so the
    frame delta between two of those lines IS delta-GFrameCounter (mod 1000, unwrapped
    here).
  * Therefore delta(tick)/delta(frame) over consecutive lines for ONE id is exactly
    the ticks-per-frame ratio Probe A reports -- recovered, not modelled.
  * CAVEAT 1: `tick` is the last INTEGRATED authority tick, read on the game thread.
    It is the publish path's own number, not the tick-mapper's; the live probe uses
    the mapper (the only game-thread-safe source) and may differ by a constant offset.
    A constant offset does not change a delta, which is all this measures.
  * CAVEAT 2: a frame that logged nothing is invisible to the frame counter. Since
    these sessions ran LogOGNet=Verbose with per-tick server lines, coverage is very
    close to complete -- but this is a proxy and the live probe is the authority.

Usage:  python t20_replay_probe_a.py <serverLog> [clientLog ...]
"""

import re
import sys
from collections import Counter

LINE = re.compile(
    r"^\[[0-9.]+-(\d\d)\.(\d\d)\.(\d\d):(\d\d\d)\]\[\s*(\d+)\](.*)$"
)
SEND = re.compile(r"\[SendCorrectionStateToClients\] id=(\d+) tick=(\d+)")
ARRIVAL = re.compile(
    r"\[RelayProbe\.Arrival\] samples=(\d+) gapCaptureTicks p50=(\d+) p99=(\d+) max=(\d+)"
)


def seconds(h, m, s, ms):
    return int(h) * 3600 + int(m) * 60 + int(s) + int(ms) / 1000.0


def parse(path):
    """Yield (time, unwrappedFrame, body) for every framed log line."""
    wraps = 0
    last = None
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            hit = LINE.match(raw)
            if not hit:
                continue
            frame = int(hit.group(5))
            if last is not None and frame < last:
                wraps += 1
            last = frame
            yield seconds(*hit.group(1, 2, 3, 4)), frame + wraps * 1000, hit.group(6)


def percentile(counter, q):
    total = sum(counter.values())
    if total == 0:
        return None
    rank = -(-q * total // 100)          # ceil, integer
    seen = 0
    for value in sorted(counter):
        seen += counter[value]
            # nearest-rank, the same definition FrameHealthProbe (T20's ServerFrameProbe, renamed T49) uses
        if seen >= rank:
            return value
    return max(counter)


def server_samples(path):
    """{id: [(time, frame, tick), ...]} from the game-thread publish line."""
    out = {}
    for when, frame, body in parse(path):
        hit = SEND.search(body)
        if hit:
            out.setdefault(int(hit.group(1)), []).append(
                (when, frame, int(hit.group(2)))
            )
    return out


def ratios(samples):
    """(perFrameHistogram, dFrame0, dFrameGt1, discontinuities) for one id."""
    hist = Counter()
    same = skipped = discont = 0
    for (_, f0, t0), (_, f1, t1) in zip(samples, samples[1:]):
        df, dt = f1 - f0, t1 - t0
        if dt < 0 or dt > 600:
            discont += 1
        elif df == 1:
            hist[dt] += 1
        elif df == 0:
            same += 1
        elif df > 1:
            skipped += 1
    return hist, same, skipped, discont


def window_ratio(samples, start, end):
    """Ticks-per-frame over one wall-clock interval, plus its p50."""
    inside = [s for s in samples if start <= s[0] <= end]
    if len(inside) < 2:
        return None
    hist = Counter()
    for (_, f0, t0), (_, f1, t1) in zip(inside, inside[1:]):
        if f1 - f0 == 1 and 0 <= t1 - t0 <= 600:
            hist[t1 - t0] += 1
    if not hist:
        return None
    frames = inside[-1][1] - inside[0][1]
    ticks = inside[-1][2] - inside[0][2]
    return {
        "p50": percentile(hist, 50),
        "p99": percentile(hist, 99),
        "mean": ticks / frames if frames else 0.0,
        "n": sum(hist.values()),
    }


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2

    server = argv[1]
    by_id = server_samples(server)
    if not by_id:
        print(f"no [SendCorrectionStateToClients] lines in {server}")
        return 1

    print(f"===== PROBE A (retroactive) -- {server} =====")
    longest = None
    for cid, samples in sorted(by_id.items()):
        hist, same, skipped, discont = ratios(samples)
        n = sum(hist.values())
        mean = sum(k * v for k, v in hist.items()) / n if n else 0.0
        print(
            f"  id={cid:<8} n={n:<6} p50={percentile(hist,50)} p90={percentile(hist,90)}"
            f" p99={percentile(hist,99)} max={max(hist) if hist else 0}"
            f" mean={mean:.3f}  dFrame0={same} dFrameGt1={skipped} discont={discont}"
        )
        if longest is None or n > sum(ratios(by_id[longest])[0].values()):
            longest = cid
    print(
        "  HOOK CADENCE: dFrame0 and dFrameGt1 are the exceptions to "
        "'OnPostPhysicsStep fires once per frame'. Zero of both == verified."
    )

    ref = by_id[longest]
    for client in argv[2:]:
        print(f"\n===== TIME-ALIGNED: {client} arrival windows vs server ratio =====")
        print("  windowEnd(s)  clientGap p50/p99   serverTicksPerFrame p50/p99/mean  n")
        previous = None
        for when, _frame, body in parse(client):
            hit = ARRIVAL.search(body)
            if not hit:
                continue
            if previous is not None:
                got = window_ratio(ref, previous, when)
                if got:
                    print(
                        f"  {when:12.1f}  {hit.group(2):>4}/{hit.group(3):<4}"
                        f"           {got['p50']:>4}/{got['p99']:<4}/{got['mean']:.2f}"
                        f"          {got['n']}"
                    )
            previous = when
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

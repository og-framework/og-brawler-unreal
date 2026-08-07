// SPDX-License-Identifier: MPL-2.0

#include "UEConnectionHandle.h"

// ---------------------------------------------------------------------------
// The RTT-source diagnostic category, declared in UEConnectionHandle.h and used
// by BOTH read sites: readRoundTripMs (server, feeds the tier) and
// ASimulationTimingRelay::OnRep_Buffer (client, feeds the prediction offset).
// (og-netcode-v2-input-relay T21.)
//
// THIS TU EXISTS FOR TWO REASONS. The obvious one is that a log category needs
// exactly one definition. The second is that UEConnectionHandle.h was
// header-only, so nothing forced it to be compiled on its own terms — it was
// only ever parsed through whichever consumer happened to include it. This file
// is its compile anchor.
// ---------------------------------------------------------------------------
DEFINE_LOG_CATEGORY(LogOGRtt);

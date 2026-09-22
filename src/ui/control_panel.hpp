#pragma once
#include "app_state.hpp"
#include "usrp_worker.hpp"
#include "recorder.hpp"

// Draws the left-side control panel.
// Returns nothing; communicates via AppState flags.
void DrawControlPanel(AppState& state, const UsrpWorker* worker, const Recorder* recorder);

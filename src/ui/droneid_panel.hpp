#pragma once
#include "app_state.hpp"
#include "usrp_worker.hpp"

// Receive-only protocol classification, validated DJI identity and scan controls.
void DrawDroneIdPanel(AppState& state, const UsrpWorker* worker);

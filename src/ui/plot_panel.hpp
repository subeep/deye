#pragma once
#include "app_state.hpp"
#include "signal_proc.hpp"

// Draws all signal plots on the right pane.
void DrawPlotPanel(AppState& state, const SignalProcessor& proc,
                   double center_freq_hz, double sample_rate_hz);

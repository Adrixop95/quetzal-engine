#pragma once

namespace aztec::engine {
// Separate command-line mode: no SDL initialization, renderer, audio or reference runtime.
int run_headless(int argc, char** argv);
}

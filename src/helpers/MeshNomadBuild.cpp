// MeshNomad: board marker "MNENV:<env>;VER:<version>;" kept in the firmware image, so a tool
// can read which board/role a .bin was built for (grep -a, no running device needed).
// MESHNOMAD_BUILD is set by [mn_release] in variants/meshnomad/platformio.ini, so MeshCore's own
// envs compile this file to nothing. `get build` uses the same MESHNOMAD_BUILD_ENV macro.
#ifdef MESHNOMAD_BUILD

#include "MeshNomadFeatures.h"  // MESHNOMAD_BUILD_ENV (falls back to "unknown")

#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "unknown"
#endif

// `used` only stops the compiler; the linker keeps it through -Wl,-u,meshnomad_mnenv
// (build_flags of [mn_release]), because --gc-sections drops unreferenced sections.
extern "C" const char meshnomad_mnenv[] __attribute__((used, section(".rodata.mnenv"))) =
  "MNENV:" MESHNOMAD_BUILD_ENV ";VER:" FIRMWARE_VERSION ";";

#endif  // MESHNOMAD_BUILD

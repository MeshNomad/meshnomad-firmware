#pragma once
// MeshNomad B87: 'region load' starts an interactive bulk load that takes every following CLI
// line as a region definition, with no reply, until a blank line ends it. Sent over LoRa it
// locked the node's CLI ('reboot' included), and the blank line that ended it replaced the
// whole region map. Rules here: a remote sender cannot start a load; lines from a remote
// sender are never taken as definitions; a line with the app's 'NN|' prefix, or a long
// silence, ends a load without touching the map. No Arduino dependency (unit-tested natively).
#include <stdint.h>
#include <string.h>

namespace mesh {

#define REGION_LOAD_IDLE_MILLIS  300000UL   // 5 min without a line: the load is given up

enum RegionLoadLine : uint8_t {
  REGION_LOAD_DEFINITION,   // a line of the load
  REGION_LOAD_ABORT,        // give the load up (map unchanged), then run the line as a command
  REGION_LOAD_PASS          // run the line as a command, the load goes on for its console
};

// "region load [...]", as CommonCLI takes it (words split by spaces)
inline bool regionLoadIsStart(const char* cmd) {
  while (*cmd == ' ') cmd++;
  if (strncmp(cmd, "region", 6) != 0 || cmd[6] != ' ') return false;
  cmd += 6;
  while (*cmd == ' ') cmd++;
  return strncmp(cmd, "load", 4) == 0 && (cmd[4] == 0 || cmd[4] == ' ');
}

// the companion app's 'NN|' command prefix. Wider than handleCommand()'s own test (more than
// 4 characters) on purpose: 'NN| ', the app's blank line, counts too. RegionMap accepts '|'
// in a name, so a region whose name has '|' as its third character cannot be bulk loaded.
inline bool regionLoadHasPrefix(const char* line) {
  while (*line == ' ') line++;
  return line[0] && line[1] && line[2] == '|';
}

// What to do with a line that arrives while a load is active. remote: it came over the mesh
// (a sender timestamp), not from a local console. idle_ms: time since the load's last line.
inline RegionLoadLine regionLoadLine(bool remote, bool prefixed, uint32_t idle_ms) {
  if (remote) return REGION_LOAD_PASS;
  if (prefixed || idle_ms >= REGION_LOAD_IDLE_MILLIS) return REGION_LOAD_ABORT;
  return REGION_LOAD_DEFINITION;
}

}  // namespace mesh

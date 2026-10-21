// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Read-only preparation of the player's own plaintext dump. No game data is bundled.
namespace UnchartedPrepare {
using Bytes=std::vector<uint8_t>;
struct Dump { Bytes eboot,libc,fios,sfo; };
struct Files { Bytes boot,content; std::string title; };
std::string CheckTitle(const Bytes& sfo);
// Produces the Uncharted BBPROBE5/BBCONT01 contracts, or throws with a reason.
Files Prepare(const Dump& dump);
}

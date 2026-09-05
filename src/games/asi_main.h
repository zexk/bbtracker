#pragma once

namespace bb {

// Each game's ASI defines this. It runs on its own thread once the DLL is
// attached, and is expected to block for the life of the process.
void asi_main();

} // namespace bb

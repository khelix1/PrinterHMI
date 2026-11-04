#pragma once

/* Includes the terminating NUL. Shared by the macro builder and semantic
 * action resolver so an accepted command is never shortened at dispatch. */
#define GCODE_COMMAND_CAPACITY 640

/* Every supported byte needs at most two JSON bytes (quote, backslash or
 * newline). Reserve room for the larger WebSocket JSON-RPC envelope as well
 * as the HTTP script envelope. Other control bytes are rejected. */
#define GCODE_COMMAND_JSON_CAPACITY (2 * (GCODE_COMMAND_CAPACITY - 1) + 128)

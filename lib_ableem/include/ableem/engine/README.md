# ableem::engine (reserved)

Empty for now. This is where portable, non-rendering library code would live if pieces of the app's
`src/code/engine/` (database access, the game scanner, serial/CD reading, etc.) are ever pulled out of
AutoBleem and into lib_ableem as reusable, platform-independent logic.

Everything the library currently provides is SDL-facing and lives under `include/ableem/ui/` instead.

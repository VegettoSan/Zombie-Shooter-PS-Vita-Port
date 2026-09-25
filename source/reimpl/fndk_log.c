/*
 * Intentionally empty.
 *
 * Zombie Shooter already provides the project's single strong FalsoNDK
 * logging bridge in source/main.c::fndk_log().  That bridge routes FalsoNDK
 * messages through logger.h, which keeps full diagnostics in Debug and strips
 * warn/debug traffic in Release while retaining errors/fatals.
 *
 * Do not add another fndk_log() implementation here: doing so creates a
 * multiple-definition linker failure because source/main.c already owns the
 * symbol.
 */

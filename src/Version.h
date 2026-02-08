#pragma once

/// Major version number.
#define WILD_VERSION_MAJOR 0
/// Minor version number.
#define WILD_VERSION_MINOR 1
/// Patch version number.
#define WILD_VERSION_PATCH 0
/// Release version number.
#define WILD_VERSION_RELEASE 0

/**
 * Stringify version components.
 *
 * @param major Major version.
 * @param minor Minor version.
 * @param patch Patch version.
 * @param release Release version.
 *
 * @see WILD_VERSION
 */
#define WILD_VERSION_STRINGIFY(major, minor, patch, release) \
	#major "." #minor "." #patch "." #release

/// Complete version string (e.g., "0.1.0.0").
#define WILD_VERSION \
	WILD_VERSION_STRINGIFY( \
		WILD_VERSION_MAJOR, \
		WILD_VERSION_MINOR, \
		WILD_VERSION_PATCH, \
		WILD_VERSION_RELEASE)


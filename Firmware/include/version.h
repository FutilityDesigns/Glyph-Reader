/*
================================================================================
  Glyph Reader - Version Information Header
================================================================================
  
  This header provides version information for the firmware.
  
  Version numbers are defined in platformio.ini and injected at build time.
  This file provides defaults and helper macros for accessing version info.
  
  Versioning follows Semantic Versioning (SemVer):
    MAJOR.MINOR.PATCH
    
    - MAJOR: Incompatible API/protocol changes, major rewrites
    - MINOR: New features, backward-compatible functionality
    - PATCH: Bug fixes, minor improvements
  
  Build Metadata (auto-generated):
    - BUILD_TIMESTAMP: Date/time of build
    - GIT_COMMIT: Git commit hash (if available)
    - BUILD_ENV: Build environment (dev/prod)
  
================================================================================
*/

#ifndef VERSION_H
#define VERSION_H

//=====================================
// Version Numbers (from platformio.ini)
//=====================================

// These are defined via -D flags in platformio.ini
// Defaults provided here in case of direct compilation

#ifndef VERSION_MAJOR
#define VERSION_MAJOR 0
#endif

#ifndef VERSION_MINOR
#define VERSION_MINOR 1
#endif

#ifndef VERSION_PATCH
#define VERSION_PATCH 0
#endif

//=====================================
// Build Metadata (auto-generated)
//=====================================

// Build timestamp - injected by PlatformIO build script
#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "Unknown"
#endif

// Git commit hash - injected by PlatformIO build script (short hash)
#ifndef GIT_COMMIT
#define GIT_COMMIT "Unknown"
#endif

// Build environment name
#if defined(ENV_PROD)
#define BUILD_ENV "prod"
#elif defined(ENV_DEV)
#define BUILD_ENV "dev"
#else
#define BUILD_ENV "unknown"
#endif

//=====================================
// Version String Helpers
//=====================================

// Stringify macros for converting numbers to strings
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Full version string: "1.2.3"
#define VERSION_STRING \
    TOSTRING(VERSION_MAJOR) "." TOSTRING(VERSION_MINOR) "." TOSTRING(VERSION_PATCH)

// Version with build environment: "1.2.3-dev" or "1.2.3-prod"
#define VERSION_STRING_FULL \
    VERSION_STRING "-" BUILD_ENV

// Complete version info for display: "1.2.3-prod (abc1234)"
#define VERSION_STRING_COMPLETE \
    VERSION_STRING_FULL " (" GIT_COMMIT ")"

//=====================================
// Version Access Functions
//=====================================

/**
 * Get the major version number
 */
inline int getVersionMajor() { return VERSION_MAJOR; }

/**
 * Get the minor version number
 */
inline int getVersionMinor() { return VERSION_MINOR; }

/**
 * Get the patch version number
 */
inline int getVersionPatch() { return VERSION_PATCH; }

/**
 * Get the version string (e.g., "1.2.3")
 */
inline const char* getVersionString() { return VERSION_STRING; }

/**
 * Get the full version string with environment (e.g., "1.2.3-prod")
 */
inline const char* getVersionStringFull() { return VERSION_STRING_FULL; }

/**
 * Get the complete version info (e.g., "1.2.3-prod (abc1234)")
 */
inline const char* getVersionStringComplete() { return VERSION_STRING_COMPLETE; }

/**
 * Get the build timestamp
 */
inline const char* getBuildTimestamp() { return BUILD_TIMESTAMP; }

/**
 * Get the git commit hash
 */
inline const char* getGitCommit() { return GIT_COMMIT; }

/**
 * Get the build environment
 */
inline const char* getBuildEnvironment() { return BUILD_ENV; }

#endif // VERSION_H

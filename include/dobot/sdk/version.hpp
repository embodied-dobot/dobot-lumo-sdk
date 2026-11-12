#pragma once

// Compatibility rules: adding a function or field is a minor version change; changing the parameters,
// return values, field meanings, joint count or topic names of an existing function is a major version
// change. Deprecate a function for one version before removing it.
#define DOBOT_SDK_VERSION_MAJOR 0
#define DOBOT_SDK_VERSION_MINOR 2
#define DOBOT_SDK_VERSION_PATCH 0

namespace dobot::sdk {
inline namespace v1 {

// Version of the library linked into the program. Compare it with the header macros to ensure the headers and the static library are the same version.
struct SdkVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

SdkVersion GetSdkVersion();

}  // namespace v1
}  // namespace dobot::sdk

include("${CMAKE_CURRENT_LIST_DIR}/Utilities.cmake")

check_generator("Xcode")

set_common_xcode_attributes()

set(REALM_SKIP_SHARED_LIB ON)
set(REALM_BUILD_LIB_ONLY ON)

set(CMAKE_SYSTEM_NAME iOS)
set(CMAKE_OSX_DEPLOYMENT_TARGET 8.0)
set(CMAKE_IOS_INSTALL_COMBINED ON)
set(CMAKE_OSX_ARCHITECTURES
    armv7
    i386
    arm64
    x86_64
)

set_bitcode_attributes()

include("${CMAKE_CURRENT_LIST_DIR}/Utilities.cmake")

check_generator("Xcode")

set_common_xcode_attributes()
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(REALM_BUILD_LIB_ONLY ON)

set(CMAKE_SYSTEM_NAME tvOS)
set(CMAKE_OSX_DEPLOYMENT_TARGET 9.0)

set(CMAKE_XCODE_ATTRIBUTE_ENABLE_BITCODE[sdk=appletv*] "YES")

set_bitcode_attributes()

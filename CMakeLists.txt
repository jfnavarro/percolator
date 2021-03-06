#############################################################################
# NAME AND VERSION
###############################################################################
cmake_minimum_required (VERSION 2.6)
project(PERCOLATOR)
set(PERCOLATOR_SOURCE_DIR ${CMAKE_SOURCE_DIR})
include("CommonCMake.txt")

###############################################################################
# PREPARING TO INSTALL
###############################################################################

my_set(CMAKE_BUILD_TYPE "Debug" "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel.")
my_set(CMAKE_PREFIX_PATH "../" "Default path to packages")

# PRINT VARIBALES TO STDOUT
MESSAGE( STATUS )
MESSAGE( STATUS
"-------------------------------------------------------------------------------"
)
MESSAGE( STATUS "Building PERCOLATOR:" )
MESSAGE( STATUS "change a configuration variable with: cmake -D<Variable>=<Value>" )
MESSAGE( STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}" )
MESSAGE( STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}" )
MESSAGE( STATUS "CMAKE_PREFIX_PATH = ${CMAKE_PREFIX_PATH}" )
MESSAGE( STATUS "GOOGLE_TEST = ${GOOGLE_TEST}" )
MESSAGE( STATUS "GOOGLE_TEST_PATH = ${GOOGLE_TEST_PATH}" )
MESSAGE( STATUS "TARGET_ARCH = ${TARGET_ARCH}" )
MESSAGE( STATUS "TOOL CHAIN FILE = ${CMAKE_TOOLCHAIN_FILE}")
MESSAGE( STATUS "PROFILING = ${PROFILING}")
MESSAGE( STATUS
"-------------------------------------------------------------------------------"
)
MESSAGE( STATUS )


# Detect machine architecture, on UNIX:
get_arch("${TARGET_ARCH}")

# STORE NEWLY SET VARIABLES IN *.h.cmake FILES
load_config_files("${CMAKE_CURRENT_SOURCE_DIR}/../")
load_config_files("${CMAKE_CURRENT_SOURCE_DIR}")

###############################################################################
# COMPILING
###############################################################################
if( APPLE )
     add_definitions(-Dxdr_uint16_t=xdr_u_int16_t -Dxdr_uint32_t=xdr_u_int32_t -Dxdr_uint64_t=xdr_u_int64_t)
endif( APPLE )

add_subdirectory(src)

###############################################################################
# TESTING
###############################################################################

# Enabling system level tests (Ctest)
enable_testing()
# Scheduling system level tests
add_subdirectory(data/system_tests/percolator)
# Scheduling unit level tests
if(GOOGLE_TEST)
  add_subdirectory(data/unit_tests/percolator)
endif()

###############################################################################
# INSTALLING
###############################################################################


if( MINGW )
    message( STATUS "    Installing system-libraries: MinGW DLLs." )
    set( CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS ${MINGW_PATH}/bin/libgcc_s_sjlj-1.dll ${MINGW_PATH}/bin/libstdc++-6.dll ${MINGW_PATH}/bin/libxerces-c-3-1.dll ${MINGW_PATH}/bin/libboost_system.dll ${MINGW_PATH}/bin/libboost_filesystem.dll)
    include( InstallRequiredSystemLibraries )
endif( MINGW )

install(FILES src/xml/percolator_in.xsd DESTINATION ${XML_REL_PATH}/${PIN_SCHEMA_LOCATION} PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
install(FILES src/xml/percolator_out.xsd DESTINATION ${XML_REL_PATH}/${POUT_SCHEMA_LOCATION} PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# ###############################################################################
# # PACKAGING
# ###############################################################################
# # PACKAGING OPTIONS: GENERAL
set(PACKAGE_NICKNAME "percolator")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Percolator and qvality, two post processors for shotgun proteomics data.")
include("CPack.txt")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libboost-filesystem (>=1.46.1), libxerces-c3.1 (>=3.1.1)")
set(CPACK_RPM_PACKAGE_DEPENDS "libc6, libgcc1")

# We assume static linkage to the xerces-c libraries for the rpms, if not uncomment next line
# set(CPACK_RPM_PACKAGE_DEPENDS "${CPACK_RPM_PACKAGE_DEPENDS}, xerces-c (>=3.1.1), boost-filesystem (>=1.46), boost-system (>=1.46), boost-math (>=1.46)")
set(CPACK_RPM_PACKAGE_DEPENDS "${CPACK_RPM_PACKAGE_DEPENDS}, boost-filesystem (>=1.46), boost-system (>=1.46), boost-math (>=1.46)")


include(CPack)

find_package( Sqlite3 REQUIRED )
if(SQLITE3_FOUND)
  message(STATUS "SQL Lite 3 found: ${SQLITE3_INCLUDE_DIR}")
else(SQLITE3_FOUND)
  message(FATAL_ERROR "The package SQL Lite 3 has not been found")
endif(SQLITE3_FOUND)
include_directories(${SQLITE3_INCLUDE_DIR})

find_package( ZLIB REQUIRED )
include_directories(${ZLIB_INCLUDE_DIRS})

add_subdirectory(RAMP)
include_directories(RAMP)

file(GLOB MSTOOLKIT_SOURCES *.cpp)
add_library (MSToolkit STATIC  ${MSTOOLKIT_SOURCES})

target_link_libraries(MSToolkit ${ZLIB_LIBRARIES} ${SQLITE3_LIBRARIES} RAMP)

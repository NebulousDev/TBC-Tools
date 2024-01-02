# CMakeList.txt : CMake project for TBC-Tools, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (TBC-Tools "TBC-Tools.cpp" "TBC-Tools.h" "TBC-IVTC.cpp" "TBC-IVTC.h" )

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET TBC-Tools PROPERTY CXX_STANDARD 20)
endif()

find_package(RapidJSON REQUIRED)
include_directories(${RapidJSON_INCLUDE_DIRS})

ADD_DEFINITIONS(-DPROJECT_SRC_DIR="${CMAKE_SOURCE_DIR}")

# TODO: Add tests and install targets if needed.

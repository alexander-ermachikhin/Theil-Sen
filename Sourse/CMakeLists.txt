cmake_minimum_required(VERSION 3.20)
project(LabVIEW_TheilSen CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Locate LabVIEW cintools on the developer's PC
if(EXISTS "C:/Program Files/National Instruments/LabVIEW 2021/cintools")
    set(LABVIEW_CINTOOLS "C:/Program Files/National Instruments/LabVIEW 2021/cintools")
elseif(EXISTS "C:/Program Files/National Instruments/LabVIEW/cintools")
    set(LABVIEW_CINTOOLS "C:/Program Files/National Instruments/LabVIEW/cintools")
else()
    set(LABVIEW_CINTOOLS "C:/Program Files/National Instruments/LabVIEW 2021/cintools")
endif()

message(STATUS "Using LabVIEW header path for compilation: ${LABVIEW_CINTOOLS}")
include_directories(${LABVIEW_CINTOOLS})

# 2. FULL MSVC EMULATION FOR GCC/MinGW
# _MSC_VER=1920 enables the Visual Studio code branch in LabVIEW headers
# _M_X64 and _M_AMD64 completely satisfy the ProcessorType checks for Win64
add_compile_definitions(
        _MSC_VER=1920
        _M_X64
        _M_AMD64
        WIN32
        _WIN32
        _WIN64
        MSWIN
)

# 3. Critical deployment flags for cross-computer compatibility
if(MINGW OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic")
endif()

# 4. Compile as a dynamic library target (.dll)
add_library(LabVIEW_TheilSen SHARED library.cpp)
set_target_properties(LabVIEW_TheilSen PROPERTIES PREFIX "")

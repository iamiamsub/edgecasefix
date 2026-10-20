cmake_minimum_required(VERSION 3.31)

# edgecasefix.dll: fixes for beatmania IIDX. Now: an 012 build forced to TDJ stops a still turntable
#   after 133 ms, as the 010 builds do.
# tt_log.dll: for testing, records the turntable and the scratch judgments of a play.
# ttlab.exe: runs the game's turntable state machine on a scripted turntable (lab/).
# tt_log's helpers (memory, modules, avs2, hooks) come from omnifix (MIT, LICENSE-omnifix.txt).
project(edgecasefix VERSION 1.0.0 LANGUAGES CXX)

find_package(Zydis CONFIG REQUIRED)
find_package(safetyhook CONFIG REQUIRED)

# edgecasefix has no C runtime and no hook library, to stay small: its own code and kernel32.
add_library(edgecasefix SHARED src/edgecasefix.cc)
target_compile_features(edgecasefix PRIVATE cxx_std_23)

if (MSVC)
    target_compile_options(edgecasefix PRIVATE /utf-8 /Brepro /GS- /Zl)
    target_link_options(edgecasefix PRIVATE /Brepro /NODEFAULTLIB /ENTRY:DllMain)
endif ()

add_library(tt_log SHARED src/tt_log.cc src/memory.cc src/modules.cc)
target_compile_features(tt_log PRIVATE cxx_std_23)
target_link_directories(tt_log PRIVATE ${CMAKE_SOURCE_DIR}/lib)
target_link_libraries(tt_log PRIVATE avs2-core safetyhook::safetyhook)

if (MSVC)
    target_compile_options(tt_log PRIVATE /utf-8 /Brepro)
    target_link_options(tt_log PRIVATE /Brepro)
endif ()

add_executable(ttlab lab/ttlab.cpp)
target_compile_features(ttlab PRIVATE cxx_std_23)

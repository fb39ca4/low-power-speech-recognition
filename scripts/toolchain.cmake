SET(CMAKE_SYSTEM_NAME Generic)
SET(CMAKE_SYSTEM_PROCESSOR arm)

# Configure toolchain

IF(NOT TARGET_TRIPLET)
    SET(TARGET_TRIPLET "arm-none-eabi")
ENDIF()

IF (WIN32)
    SET(TOOL_EXECUTABLE_SUFFIX ".exe")
ELSE()
    SET(TOOL_EXECUTABLE_SUFFIX "")
ENDIF()

IF(${CMAKE_VERSION} VERSION_LESS 3.6.0)
    INCLUDE(CMakeForceCompiler)
    CMAKE_FORCE_C_COMPILER("${TARGET_TRIPLET}-gcc${TOOL_EXECUTABLE_SUFFIX}" GNU)
    CMAKE_FORCE_CXX_COMPILER("${TARGET_TRIPLET}-g++${TOOL_EXECUTABLE_SUFFIX}" GNU)
ELSE()
    SET(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    SET(CMAKE_C_COMPILER "${TARGET_TRIPLET}-gcc${TOOL_EXECUTABLE_SUFFIX}" CACHE STRING "Cross-compiler")
    SET(CMAKE_CXX_COMPILER "${TARGET_TRIPLET}-g++${TOOL_EXECUTABLE_SUFFIX}" CACHE STRING "Cross-compiler")
ENDIF()

SET(CMAKE_LINKER       "${TARGET_TRIPLET}-g++${TOOL_EXECUTABLE_SUFFIX}" CACHE INTERNAL "linker tool")
SET(CMAKE_ASM_COMPILER "${TARGET_TRIPLET}-gcc${TOOL_EXECUTABLE_SUFFIX}" CACHE INTERNAL "ASM compiler")
SET(CMAKE_OBJCOPY      "${TARGET_TRIPLET}-objcopy${TOOL_EXECUTABLE_SUFFIX}" CACHE INTERNAL "objcopy tool")
SET(CMAKE_OBJDUMP      "${TARGET_TRIPLET}-objdump${TOOL_EXECUTABLE_SUFFIX}" CACHE INTERNAL "objdump tool")
SET(CMAKE_SIZE         "${TARGET_TRIPLET}-size${TOOL_EXECUTABLE_SUFFIX}" CACHE INTERNAL "size tool")
SET(CMAKE_DEBUGER      "${TARGET_TRIPLET}-gdb${TOOL_EXECUTABLE_SUFFIX}" CACHE INTERNAL "debuger")
SET(CMAKE_CPPFILT      "${TARGET_TRIPLET}-c++filt${TOOL_EXECUTABLE_SUFFIX}" CACHE INTERNAL "C++filt")
SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
SET(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Prepare linker script

SET(LINKER_SCRIPT_NAME linkerscript.ld)
SET(LINKER_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/${LINKER_SCRIPT_NAME})
CONFIGURE_FILE(scripts/${LINKER_SCRIPT_NAME} ${LINKER_SCRIPT})

# Toolchain configuration
SET(CCFLAGS "\
    -fdata-sections \
    -ffunction-sections \
    -finline-limit=10000 \
    -fshort-wchar \
    -fsingle-precision-constant \
    -funsigned-bitfields \
    -funsigned-char \
    -fwrapv \
    -g3 \
    -gdwarf \
    -W \
    -Wall \
    -Wduplicated-cond \
    -Werror=format \
    -Werror=maybe-uninitialized \
    -Werror=overflow \
    -Werror=sign-compare \
    -Wextra \
    -Wlogical-op \
    -Wpointer-arith \
    -Wundef \
")

SET(CCFLAGS_RELEASE "\
    -O3 \
")

SET(CCFLAGS_DEBUG "\
    -fno-move-loop-invariants \
    -fno-split-wide-types \
    -fno-tree-loop-optimize \
    -Og \
")


SET(CFLAGS "\
    -std=gnu11 \
    -Wbad-function-cast \
    -Wimplicit \
    -Wnested-externs \
    -Wredundant-decls \
    -Wstrict-prototypes \
")


SET(CXXFLAGS "\
    -fno-exceptions \
    -fno-rtti \
    -fno-threadsafe-statics \
    -fno-unwind-tables \
    -fstrict-enums \
    -fuse-cxa-atexit \
    -std=c++17 \
    -Woverloaded-virtual \
")


SET(ASFLAGS "\
    -g3 \
    -gdwarf \
")


SET(ARCHFLAGS "\
    -mcpu=cortex-m4 \
    -mfloat-abi=hard \
    -mfpu=fpv4-sp-d16 \
    -mthumb \
")


SET(LINKFLAGS "\
    --specs=nano.specs \
    --specs=nosys.specs \
    -Llink \
    -nostartfiles \
    -Tlinkerscript.ld \
    -Wl,--fatal-warnings \
    -Wl,--gc-sections \
    -Wl,--no-wchar-size-warning \
    -Wl,--relax \
    -Wl,-Map,${CMAKE_PROJECT_NAME}.map,--cref \
    -Wl,-wrap,_calloc_r \
    -Wl,-wrap,_free_r \
    -Wl,-wrap,_malloc_r \
    -Wl,-wrap,_realloc_r \
")


# Set flags for CMake
SET(CMAKE_C_FLAGS "${ARCHFLAGS} ${CCFLAGS} ${CFLAGS}" CACHE INTERNAL "c compiler flags")
SET(CMAKE_C_FLAGS_RELEASE   "${CCFLAGS_RELEASE}"      CACHE INTERNAL "c compiler flags release")
SET(CMAKE_C_FLAGS_DEBUG     "${CCFLAGS_DEBUG}"        CACHE INTERNAL "c compiler flags debug")

SET(CMAKE_CXX_FLAGS "${ARCHFLAGS} ${CCFLAGS} ${CXXFLAGS}" CACHE INTERNAL "cxx compiler flags")
SET(CMAKE_CXX_FLAGS_RELEASE "${CCFLAGS_RELEASE}"          CACHE INTERNAL "cxx compiler flags release")
SET(CMAKE_CXX_FLAGS_DEBUG   "${CCFLAGS_DEBUG}"            CACHE INTERNAL "cxx compiler flags debug")

SET(CMAKE_ASM_FLAGS "${ARCHFLAGS} ${ASFLAGS}" CACHE INTERNAL "asm compiler flags")

SET(CMAKE_EXE_LINKER_FLAGS  "${ARCHFLAGS} ${LINKFLAGS}" CACHE INTERNAL "linker flags")

IF(APPLE)
	STRING(REPLACE "-Wl,-search_paths_first" "" CMAKE_C_LINK_FLAGS ${CMAKE_C_LINK_FLAGS})
	STRING(REPLACE "-Wl,-search_paths_first" "" CMAKE_CXX_LINK_FLAGS ${CMAKE_CXX_LINK_FLAGS})
ENDIF()

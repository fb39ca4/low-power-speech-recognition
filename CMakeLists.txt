CMAKE_MINIMUM_REQUIRED(VERSION 3.9)

INCLUDE(scripts/toolchain.cmake)

PROJECT("lpsr")

FILE(GLOB_RECURSE ASM_FILES *.sx)
SET_SOURCE_FILES_PROPERTIES(${ASM_FILES} PROPERTIES LANGUAGE CXX)

MACRO(GET_SOURCES OUTPUT SRC_DIR)
	FILE(GLOB_RECURSE ${OUTPUT} ${SRC_DIR}/*.c ${SRC_DIR}/*.cpp ${SRC_DIR}/*.h ${SRC_DIR}/*.hpp ${SRC_DIR}/*.sx)
ENDMACRO(GET_SOURCES)

INCLUDE_DIRECTORIES(
	modm/ext
	modm/ext/cmsis/core
	modm/ext/cmsis/device
	modm/ext/cmsis/dsp
	modm/src
	gcem/include
	src
)
INCLUDE_DIRECTORIES(SYSTEM include)

MESSAGE(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
MESSAGE(STATUS "Build dir: ${PROJECT_BINARY_DIR}")

SET(DEFAULT_HANDLER_SRC modm/src/modm/platform/core/default_handler.sx)

GET_SOURCES(MODM_SRC modm/src)

GET_SOURCES(COMMON_SRC src/common)

GET_SOURCES(ARM_DSP_SRC modm/ext/cmsis/dsp)

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/voice_command_data.cpp
	COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/voice_commands_to_cpp.py ${CMAKE_CURRENT_BINARY_DIR}/voice_command_data.cpp
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS data/words.txt ${CMAKE_CURRENT_SOURCE_DIR}/scripts/voice_commands_to_cpp.py
)

SET(LAUNCH_JSON "{\n    \"version\": \"0.2.0\",\n    \"configurations\": [\n")

GET_SOURCES(APP_SRC src/app)

ADD_EXECUTABLE(${PROJECT_NAME} ${APP_SRC} ${COMMON_SRC} ${MODM_SRC} ${ARM_DSP_SRC} ${CMAKE_CURRENT_BINARY_DIR}/voice_command_data.cpp)

SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
	TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC "MODM_DEBUG_BUILD")
ENDIF()

#TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC "__FPU_PRESENT=1")

ADD_CUSTOM_COMMAND(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_SIZE} ARGS --format=sysv ${PROJECT_NAME}.elf)
ADD_CUSTOM_COMMAND(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_SIZE} ARGS --format=berkeley ${PROJECT_NAME}.elf)
ADD_CUSTOM_COMMAND(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_OBJCOPY} -Oihex ${PROJECT_NAME}.elf ${PROJECT_NAME}.hex)
ADD_CUSTOM_COMMAND(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_OBJCOPY} -Obinary ${PROJECT_NAME}.elf ${PROJECT_NAME}.bin)
ADD_CUSTOM_COMMAND(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_OBJDUMP} -x -s -S -l -w ${PROJECT_NAME}.elf > ${PROJECT_NAME}.lss)

ADD_CUSTOM_TARGET(upload openocd -f "${PROJECT_SOURCE_DIR}/scripts/openocd.cfg" -c "program_file ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.elf" DEPENDS ${PROJECT_NAME})

MESSAGE(STATUS "added ${PROJECT_NAME}")

STRING(APPEND LAUNCH_JSON "        {\n\
		\"type\": \"cortex-debug\",\n\
		\"request\": \"launch\",\n\
		\"servertype\": \"openocd\",\n\
		\"cwd\": \"\${workspaceRoot}\",\n\
		\"executable\": \"${PROJECT_BINARY_DIR}/${PROJECT_NAME}.elf\",\n\
		\"name\": \"${PROJECT_NAME} - ${CMAKE_BUILD_TYPE} Build\",\n\
		\"device\": \"STM32F401RE\",\n\
		\"configFiles\": [\"\${workspaceRoot}/scripts/openocd.cfg\"],\n\
	}")

ADD_CUSTOM_TARGET(build-type ALL cmake -E echo "Build type: ${CMAKE_BUILD_TYPE}")

STRING(REPLACE "}        {" "},\n        {" LAUNCH_JSON ${LAUNCH_JSON})
STRING(APPEND LAUNCH_JSON "\n    ]\n}\n")
FILE(WRITE ${PROJECT_SOURCE_DIR}/.vscode/launch.json ${LAUNCH_JSON})
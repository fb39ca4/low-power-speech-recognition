CMAKE_MINIMUM_REQUIRED(VERSION 3.9)

INCLUDE(scripts/toolchain.cmake)

PROJECT("dolken")

FILE(GLOB_RECURSE ASM_FILES *.sx)
SET_SOURCE_FILES_PROPERTIES(${ASM_FILES} PROPERTIES LANGUAGE CXX)

MACRO(GET_SOURCES OUTPUT SRC_DIR)
	FILE(GLOB_RECURSE ${OUTPUT} ${SRC_DIR}/*.c ${SRC_DIR}/*.cpp ${SRC_DIR}/*.h ${SRC_DIR}/*.hpp ${SRC_DIR}/*.sx)
ENDMACRO(GET_SOURCES)

MACRO(GET_CHILD_DIRS OUTPUT DIR)
	FILE(GLOB DIR_CONTENTS RELATIVE ${DIR} ${DIR}/*)
	SET(${OUTPUT} "")
	FOREACH(CHILD ${DIR_CONTENTS})
		IF(IS_DIRECTORY ${DIR}/${CHILD})
			LIST(APPEND ${OUTPUT} ${CHILD})
		ENDIF()

	ENDFOREACH()
ENDMACRO(GET_CHILD_DIRS)

GET_CHILD_DIRS(BOARD_NAMES ${PROJECT_SOURCE_DIR}/src/boards)

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
#ADD_LIBRARY(modm OBJECT ${MODM_SRC})

GET_SOURCES(COMMON_SRC src/common)
#ADD_LIBRARY(dolken OBJECT ${COMMON_SRC})

GET_SOURCES(ARM_DSP_SRC modm/ext/cmsis/dsp)

add_custom_command(
    OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/voice_command_data.cpp
	COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/scripts/voice_commands_to_cpp.py ${CMAKE_CURRENT_BINARY_DIR}/voice_command_data.cpp
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS data/voice.txt data/words.txt ${CMAKE_CURRENT_SOURCE_DIR}/scripts/voice_commands_to_cpp.py
)

SET(LAUNCH_JSON "{\n    \"version\": \"0.2.0\",\n    \"configurations\": [\n")

FOREACH(BOARD_NAME ${BOARD_NAMES})
	GET_SOURCES(BOARD_SRC src/boards/${BOARD_NAME})

	ADD_EXECUTABLE(${BOARD_NAME} ${BOARD_SRC} ${COMMON_SRC} ${MODM_SRC} ${ARM_DSP_SRC} ${CMAKE_CURRENT_BINARY_DIR}/voice_command_data.cpp)

	SET_TARGET_PROPERTIES(${BOARD_NAME} PROPERTIES SUFFIX ".elf")

	IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
		TARGET_COMPILE_DEFINITIONS(${BOARD_NAME} PUBLIC "MODM_DEBUG_BUILD")
	ENDIF()

	#TARGET_COMPILE_DEFINITIONS(${BOARD_NAME} PUBLIC "__FPU_PRESENT=1")

	ADD_CUSTOM_COMMAND(TARGET ${BOARD_NAME} POST_BUILD COMMAND ${CMAKE_SIZE} ARGS --format=sysv ${BOARD_NAME}.elf)
	ADD_CUSTOM_COMMAND(TARGET ${BOARD_NAME} POST_BUILD COMMAND ${CMAKE_SIZE} ARGS --format=berkeley ${BOARD_NAME}.elf)
	ADD_CUSTOM_COMMAND(TARGET ${BOARD_NAME} POST_BUILD COMMAND ${CMAKE_OBJCOPY} -Oihex ${BOARD_NAME}.elf ${BOARD_NAME}.hex)
	ADD_CUSTOM_COMMAND(TARGET ${BOARD_NAME} POST_BUILD COMMAND ${CMAKE_OBJCOPY} -Obinary ${BOARD_NAME}.elf ${BOARD_NAME}.bin)
	ADD_CUSTOM_COMMAND(TARGET ${BOARD_NAME} POST_BUILD COMMAND ${CMAKE_OBJDUMP} -x -s -S -l -w ${BOARD_NAME}.elf > ${BOARD_NAME}.lss)

	ADD_CUSTOM_TARGET(upload-${BOARD_NAME} openocd -f "${PROJECT_SOURCE_DIR}/scripts/openocd.cfg" -c "program_file ${PROJECT_BINARY_DIR}/${BOARD_NAME}.elf" DEPENDS ${BOARD_NAME})

	MESSAGE(STATUS "added ${BOARD_NAME}")

	STRING(APPEND LAUNCH_JSON "        {\n\
            \"type\": \"cortex-debug\",\n\
            \"request\": \"launch\",\n\
            \"servertype\": \"openocd\",\n\
            \"cwd\": \"\${workspaceRoot}\",\n\
            \"executable\": \"${PROJECT_BINARY_DIR}/${BOARD_NAME}.elf\",\n\
            \"name\": \"${BOARD_NAME} - ${CMAKE_BUILD_TYPE} Build\",\n\
            \"device\": \"STM32F401RE\",\n\
            \"configFiles\": [\"\${workspaceRoot}/scripts/openocd.cfg\"],\n\
		}")
ENDFOREACH()

ADD_CUSTOM_TARGET(modm-docs lbuild build -m ":docs" WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
ADD_CUSTOM_TARGET(build-type ALL cmake -E echo "Build type: ${CMAKE_BUILD_TYPE}")

STRING(REPLACE "}        {" "},\n        {" LAUNCH_JSON ${LAUNCH_JSON})
STRING(APPEND LAUNCH_JSON "\n    ]\n}\n")
FILE(WRITE ${PROJECT_SOURCE_DIR}/.vscode/launch.json ${LAUNCH_JSON})

execute_process(COMMAND cmake -E touch ${PROJECT_BINARY_DIR}/stamp WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
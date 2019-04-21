CMAKE_GENERATOR = Unix Makefiles
BUILD_DIR=./build
BUILD_TYPE=Debug
J=1

ECHO = cmake -E echo
RMDIR = cmake -E remove_directory
TOUCH = cmake -E touch

.DEFAULT_GOAL := help

help:
	@$(ECHO) "DanSTAR RICHARD build system"
	@$(ECHO) "Current shell: $(SHELL)"
	@$(ECHO) "Usage: make <target> [J=<cpu_cores>] [BUILD_TYPE={Debug|Release}]"
	@$(ECHO) ""
	@$(ECHO) "Option J: Number of CPU cores to use while building."
	@$(ECHO) ""
	@$(ECHO) "Option BUILD_TYPE: Choose a Debug or Release build. Must capitalize exactly."
	@$(ECHO) "    Debug: No optimizations, fast compile, slow execution"
	@$(ECHO) "    Release: With optimizations, slow compile, fast execution"
	@$(ECHO) ""
	@$(ECHO) "Target can be any of the following:"
	@$(ECHO) ""
	@$(ECHO) "modm-docs"
	@$(ECHO) "    Generates HTML of modm documentation, can be found in docs directory."
	@$(ECHO) "    Dependencies:"
	@$(ECHO) "        File project.xml"
	@$(ECHO) ""
	@$(ECHO) "build"
	@$(ECHO) "    Builds firmware for all boards in src/boards."
	@$(ECHO) ""
	@$(ECHO) "build-<boardname>"
	@$(ECHO) "    Builds firmware for a single board in src/boards/<boardname>."
	@$(ECHO) ""
	@$(ECHO) "upload-<boardname>"
	@$(ECHO) "    Builds and uploads firmware for <boardname> to microcontroller."
	@$(ECHO) "    Firmware is automatically built if necessary."
	@$(ECHO) ""
	@$(ECHO) "clean"
	@$(ECHO) "    Deletes the build folder."
	@$(ECHO) ""
	@$(ECHO) "Example: Running 'make upload-mainboard J=4 BUILD_TYPE=Release' from a clean"
	@$(ECHO) "repository will build the firmware for the main board in release mode using"
	@$(ECHO) "4 CPU cores and upload it to the microcontroller."


clean:
	@$(RMDIR) build

modm/docs/stamp: project.xml
	@lbuild build -m ":docs"
	@$(TOUCH) modm/docs/stamp
	@cmake -E create_symlink ../modm/docs/html/index.html docs/modm.html

modm-docs: modm/docs/stamp

$(BUILD_DIR)/Debug/stamp: CMakeLists.txt scripts/toolchain.cmake project.xml
	@cmake -E make_directory $(BUILD_DIR)/Debug
	@cd $(BUILD_DIR)/Debug && cmake $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=Debug -G "$(CMAKE_GENERATOR)" ../..

$(BUILD_DIR)/Release/stamp: CMakeLists.txt scripts/toolchain.cmake project.xml
	@cmake -E make_directory $(BUILD_DIR)/Release
	@cd $(BUILD_DIR)/Release && cmake $(CMAKE_FLAGS) -DCMAKE_BUILD_TYPE=Release -G "$(CMAKE_GENERATOR)" ../..

build: $(BUILD_DIR)/$(BUILD_TYPE)/stamp
	@cmake --build $(BUILD_DIR)/$(BUILD_TYPE) --parallel 2

build-%: $(BUILD_DIR)/$(BUILD_TYPE)/stamp
	@cmake --build $(BUILD_DIR)/$(BUILD_TYPE) --target $*

upload-%: $(BUILD_DIR)/$(BUILD_TYPE)/stamp
	@cmake --build $(BUILD_DIR)/$(BUILD_TYPE) --target upload-$*

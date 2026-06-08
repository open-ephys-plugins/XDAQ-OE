cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0087 NEW)
if (NOT DEFINED GUI_BASE_DIR)
	if (DEFINED ENV{GUI_BASE_DIR})
		set(GUI_BASE_DIR $ENV{GUI_BASE_DIR})
	else()
		set(GUI_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../plugin-GUI)
	endif()
endif()
get_filename_component(GUI_BASE_DIR ${GUI_BASE_DIR} ABSOLUTE)

set(PLUGIN_NAME XDAQ-OE)

set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake
    ${CMAKE_MODULE_PATH}
)

# set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architecture for Mac OS X" FORCE)

project(OE_PLUGIN_${PLUGIN_NAME} VERSION 0.4.4 LANGUAGES CXX)

set(CMAKE_SHARED_LIBRARY_PREFIX "")
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(LINUX 1)
	if(NOT CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE Debug)
	endif()
endif()

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
	OEPLUGIN
	"$<$<PLATFORM_ID:Windows>:JUCE_API=__declspec(dllimport)>"
	$<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
	$<$<PLATFORM_ID:Linux>:JUCE_DISABLE_NATIVE_FILECHOOSERS=1>
	$<$<CONFIG:Debug>:DEBUG=1>
	$<$<CONFIG:Debug>:_DEBUG=1>
	$<$<CONFIG:Release>:NDEBUG=1>
	)


set(SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/Source)
file(GLOB_RECURSE SRC_FILES LIST_DIRECTORIES false "${SOURCE_PATH}/*.cpp" "${SOURCE_PATH}/*.h")
set(GUI_COMMONLIB_DIR ${GUI_BASE_DIR}/installed_libs)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_CXX_STANDARD 23)

set(CONFIGURATION_FOLDER $<$<CONFIG:Debug>:Debug>$<$<NOT:$<CONFIG:Debug>>:Release>)

list(APPEND CMAKE_PREFIX_PATH ${GUI_COMMONLIB_DIR} ${GUI_COMMONLIB_DIR}/${CONFIGURATION_FOLDER})

if (APPLE)
	add_library(${PLUGIN_NAME} MODULE ${SRC_FILES})
else()
	add_library(${PLUGIN_NAME} SHARED ${SRC_FILES})
endif()

target_compile_definitions(${PLUGIN_NAME} PRIVATE
	PLUGIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	PLUGIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	PLUGIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_compile_definitions(${PLUGIN_NAME} PRIVATE
	$<$<PLATFORM_ID:Linux>:OS_LINUX>
	$<$<PLATFORM_ID:Windows>:OS_WINDOWS>
	$<$<PLATFORM_ID:Darwin>:OS_MACOS>
)

target_compile_features(${PLUGIN_NAME} PUBLIC cxx_auto_type cxx_generalized_initializers)
target_compile_options(${PLUGIN_NAME} PRIVATE $<$<PLATFORM_ID:Darwin>:-fexperimental-library>)
target_compile_options(${PLUGIN_NAME} PRIVATE $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-deprecated-declarations>)
target_compile_options(${PLUGIN_NAME} PRIVATE $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-deprecated>)


target_include_directories(${PLUGIN_NAME} PUBLIC ${GUI_BASE_DIR}/JuceLibraryCode ${GUI_BASE_DIR}/JuceLibraryCode/modules ${GUI_BASE_DIR}/Plugins/Headers ${GUI_COMMONLIB_DIR}/include)

if (NOT CMAKE_LIBRARY_ARCHITECTURE)
	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(CMAKE_LIBRARY_ARCHITECTURE "x64")
	else()
		set(CMAKE_LIBRARY_ARCHITECTURE "x86")
	endif()
endif()

set(PLUGIN_INSTALL_DIR plugins CACHE STRING "Plugin installation directory")
set(SHARED_INSTALL_DIR shared CACHE STRING "Shared installation directory")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
		set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE C:/ProgramData/Open\ Ephys)
		set(PLUGIN_INSTALL_DIR plugins-api10 CACHE STRING "Plugin installation directory" FORCE)
		set(SHARED_INSTALL_DIR shared-api10 CACHE STRING "Shared installation directory" FORCE)
	elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
		set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE $ENV{HOME}/Library/Application\ Support/open-ephys)
		set(PLUGIN_INSTALL_DIR plugins-api10 CACHE STRING "Plugin installation directory" FORCE)
		set(SHARED_INSTALL_DIR shared-api10 CACHE STRING "Shared installation directory" FORCE)
	elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
		set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE ${GUI_BASE_DIR}/Build)
		set(PLUGIN_INSTALL_DIR ${CONFIGURATION_FOLDER}/plugins CACHE STRING "Plugin installation directory" FORCE)
		set(SHARED_INSTALL_DIR ${CONFIGURATION_FOLDER}/shared CACHE STRING "Shared installation directory" FORCE)
	endif()
endif()


# target_compile_options(${PLUGIN_NAME} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)
# target_link_libraries(${PLUGIN_NAME} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)

#Libraries and compiler options
if(MSVC)
	target_compile_definitions(${PLUGIN_NAME} PRIVATE NOMINMAX)
	target_link_libraries(${PLUGIN_NAME} PRIVATE ${GUI_BASE_DIR}/Build/${CONFIGURATION_FOLDER}/open-ephys.lib)
	target_compile_options(${PLUGIN_NAME} PRIVATE /sdl- /W0)
elseif(LINUX)
	set_target_properties(${PLUGIN_NAME} PROPERTIES
		CXX_VISIBILITY_PRESET hidden
		POSITION_INDEPENDENT_CODE ON
	)
	target_link_libraries(${PLUGIN_NAME} PRIVATE GL X11 Xext Xinerama asound dl freetype pthread rt)
	set_property(TARGET ${PLUGIN_NAME} APPEND_STRING PROPERTY LINK_FLAGS "-rdynamic")
	target_compile_options(${PLUGIN_NAME} PRIVATE -rdynamic)
elseif(APPLE)
	set_target_properties(${PLUGIN_NAME} PROPERTIES BUNDLE TRUE)
	set_property(TARGET ${PLUGIN_NAME} APPEND_STRING PROPERTY LINK_FLAGS "-undefined dynamic_lookup")

	set_target_properties(${PLUGIN_NAME} PROPERTIES
		XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
		XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED NO
	)
endif()


#create filters for vs and xcode

foreach( src_file IN ITEMS ${SRC_FILES})
	get_filename_component(src_path "${src_file}" PATH)
	file(RELATIVE_PATH src_path_rel "${SOURCE_PATH}" "${src_path}")
	string(REPLACE "/" "\\" group_name "${src_path_rel}")
	source_group("${group_name}" FILES "${src_file}")
endforeach()

# additional libraries
find_package(xdaq REQUIRED)
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)

target_link_libraries(${PLUGIN_NAME} PRIVATE
	fmt::fmt
	xdaq::xdaq_device
	nlohmann_json::nlohmann_json
)


install(TARGETS ${PLUGIN_NAME}
	LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR}
	RUNTIME DESTINATION ${PLUGIN_INSTALL_DIR}
)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	xdaq_install_core(${SHARED_INSTALL_DIR} ${SHARED_INSTALL_DIR})
	xdaq_install_device_managers(${SHARED_INSTALL_DIR}/${PLUGIN_NAME}/managers)
	set(CPACK_PACKAGE_NAME ${PLUGIN_NAME})
	set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR}/package)

	set(CPACK_NSIS_DISPLAY_NAME ${PLUGIN_NAME})
	set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
	set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
	set(CPACK_NSIS_UNINSTALL_NAME "Uninstall ${PLUGIN_NAME}")
	set(CPACK_PACKAGE_VENDOR "KonteX Neuroscience")
	string(REPLACE "/" "\\\\" CPACK_NSIS_INSTALL_ROOT ${CMAKE_INSTALL_PREFIX})
	set(CPACK_PACKAGE_INSTALL_DIRECTORY "")
	include(CPACK)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set_target_properties(${PLUGIN_NAME} PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "@loader_path/../PlugIns"
	)
	xdaq_install_core(
		${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}.bundle/Contents/PlugIns
	 	${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}.bundle/Contents/PlugIns
	)
	xdaq_install_device_managers(${PLUGIN_INSTALL_DIR}/${PLUGIN_NAME}.bundle/Contents/PlugIns/managers)
	set(CPACK_GENERATOR "DragNDrop")
	set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK ON)
    set(CPACK_PRE_BUILD_SCRIPTS "${CMAKE_SOURCE_DIR}/cmake/codesign.cmake")
	include(CPACK)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set_target_properties(${PLUGIN_NAME} PROPERTIES
		BUILD_WITH_INSTALL_RPATH TRUE
		INSTALL_RPATH "$ORIGIN/../shared/${PLUGIN_NAME}"
	)
	xdaq_install_core(${SHARED_INSTALL_DIR} ${SHARED_INSTALL_DIR})
	xdaq_install_device_managers(${SHARED_INSTALL_DIR}/${PLUGIN_NAME}/managers)
endif()

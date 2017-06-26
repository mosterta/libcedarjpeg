cmake_minimum_required(VERSION 2.8.8)

project(cedarJpeg)

set(LIBPREFIX $(PROJECT_NAME))

set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
message(STATUS "PROJECT_VERSION: ${PROJECT_VERSION}")

set(LIBCEDARJPEG_DESCRIPTION "A library to decode jpeg images with the help of the Allwinner JPEG decoder hardware")
set(LIBCEDARJPEG_URL "https://github.com/mosterta/libcedarjpeg")

include(CMakePackageConfigHelpers)	# cmake 2.8.8
include(FeatureSummary)
include(GNUInstallDirs) # cmake 2.8.5

add_definitions(-fPIC -Wall -Wextra -DUSE_UMP)

add_subdirectory("src")

# pkg-config stuff
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/libcedarjpeg.pc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/libcedarjpeg.pc"
	@ONLY
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/libcedarjpeg.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

# feature summary
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
feature_summary(WHAT ALL)


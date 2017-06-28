
include_directories(
	"${CMAKE_CURRENT_BINARY_DIR}"
)

set(LIBCEDARJPEG_SRC 
		enc_main.c
		jpeg.c
		main.c
		veavc.c
		veisp.c
		vejpeg.c 
)

# create binary
add_library(${PROJECT_NAME}-shared SHARED ${LIBCEDARJPEG_SRC})
set_target_properties(${PROJECT_NAME}-shared PROPERTIES
	OUTPUT_NAME	${PROJECT_NAME}
	SOVERSION	${PROJECT_VERSION_MAJOR}
	VERSION		${PROJECT_VERSION}
)
set(LIBS  -lpthread -lEGL -lGLESv2 -lyuv)
set(COMMON_COMPILE_DEFINITIONS "-fPIC -Wall -Wextra -DUSE_UMP")

target_link_libraries(${PROJECT_NAME}-shared ${LIBS})

set(${PROJECT_NAME}-shared-CC "${COMMON_COMPILE_DEFINITIONS} -fvisibility=hidden")

set_target_properties(${PROJECT_NAME}-shared PROPERTIES COMPILE_FLAGS "${${PROJECT_NAME}-shared-CC}")

add_executable(${PROJECT_NAME}-test ${LIBCEDARJPEG_SRC})
set_target_properties(${PROJECT_NAME}-test PROPERTIES
	OUTPUT_NAME	${PROJECT_NAME}-test
)

set(DEFINITIONS-test-CC "${COMMON_COMPILE_DEFINITIONS} -DINCLUDE_MAIN")

target_link_libraries(${PROJECT_NAME}-test ${LIBS})
set_target_properties(${PROJECT_NAME}-test PROPERTIES COMPILE_FLAGS "${DEFINITIONS-test-CC}")

find_package(JPEG REQUIRED)
if(JPEG_FOUND)
  include_directories(${JPEG_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME}-shared ${JPEG_LIBRARIES})
  target_link_libraries(${PROJECT_NAME}-test ${JPEG_LIBRARIES})
endif()

find_package(PkgConfig REQUIRED)

pkg_check_modules(PC_CEDAR_ACCESS REQUIRED cedar_access)
set(CEDAR_ACCESS_DEFINITIONS ${PC_CEDAR_ACCESS_CFLAGS_OTHER})

find_path(CEDAR_ACCESS_INCLUDE_DIR NAMES ve.h HINTS ${PC_CEDAR_ACCESS_INCLUDEDIRS} ${PC_CEDAR_ACCESS_INCLUDE_DIRS})
find_library(CEDAR_ACCESS_LIBRARY NAMES cedar_access HINTS {PC_CEDAR_ACCESS_LIBDIRS} ${PC_CEDAR_ACCESS_LIBRARY_DIRS})
find_package_handle_standard_args(CEDAR_ACCESS  DEFAULT_MSG
                                  CEDAR_ACCESS_LIBRARY CEDAR_ACCESS_INCLUDE_DIR)

set(CEDAR_ACCESS_INCLUDE_DIRS ${CEDAR_ACCESS_INCLUDE_DIR})
set(CEDAR_ACCESS_LIBRARIES ${CEDAR_ACCESS_LIBRARY})
mark_as_advanced(CEDAR_ACCESS_INCLUDE_DIRS CEDAR_ACCESS_LIBRARIES)

if(CEDAR_ACCESS_FOUND)
  include_directories(${CEDAR_ACCESS_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME}-shared ${CEDAR_ACCESS_LIBRARIES})
  target_link_libraries(${PROJECT_NAME}-test ${CEDAR_ACCESS_LIBRARIES})
endif()

pkg_check_modules(PC_UMP REQUIRED libump)
set(UMP_DEFINITIONS ${PC_UMP_CFLAGS_OTHER})

find_path(UMP_INCLUDE_DIR NAMES ump/ump.h HINTS ${PC_UMP_INCLUDEDIRS} ${PC_UMP_INCLUDE_DIRS})
find_library(UMP_LIBRARY NAMES UMP HINTS {PC_UMP_LIBDIRS} ${PC_UMP_LIBRARY_DIRS})
find_package_handle_standard_args(UMP  DEFAULT_MSG
                                  UMP_LIBRARY UMP_INCLUDE_DIR)

set(UMP_INCLUDE_DIRS ${UMP_INCLUDE_DIR})
set(UMP_LIBRARIES ${UMP_LIBRARY})
mark_as_advanced(UMP_INCLUDE_DIRS UMP_LIBRARIES)

if(UMP_FOUND)
  include_directories(${UMP_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME}-shared ${UMP_LIBRARIES})
  target_link_libraries(${PROJECT_NAME}-test ${UMP_LIBRARIES})
endif()

# install binary
install(TARGETS ${PROJECT_NAME}-shared
	EXPORT ${PROJECT_NAME}-targets
	LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)


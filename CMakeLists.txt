# Copyright (c) 2015 Klaralvdalens Datakonsult AB (KDAB).
# reserved. Use of this source code is governed by a BSD-style license that
# can be found in the LICENSE file.

cmake_minimum_required(VERSION 2.8.12.1)

if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cef")
  message(FATAL_ERROR "The `cef` subfolder is missing. Download CEF from https://cefbuilds.com/ and extract its contents to `${CMAKE_CURRENT_SOURCE_DIR}/cef`.")
endif()

# Add cef as a subdirectory.
add_subdirectory(cef)

# Add cef variables to the current scope.
get_directory_property(_CEF_VARS DIRECTORY cef VARIABLES)
foreach(_CEF_VAR ${_CEF_VARS})
  if (NOT _CEF_VAR MATCHES "^CMAKE_CURRENT_")
    get_directory_property(${_CEF_VAR} DIRECTORY cef DEFINITION ${_CEF_VAR})
  endif()
endforeach()

# Init project variables for PhantomJS-cef.
project(PhantomJS-cef)
include_directories(${PROJECT_SOURCE_DIR}/cef )

#json11 configuration
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(json11 json11/json11.cpp)
target_include_directories(json11 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/json11)
target_compile_options(json11
  PRIVATE -fno-rtti -fno-exceptions -Wall -Wextra -Werror)
configure_file("json11/json11.pc.in" "json11.pc" @ONLY)


#
# Source files.
#

# phantomjs sources.
set(PHANTOMJS_SRCS
  app.cpp
  handler.cpp
  print_handler.cpp
  main.cpp
  debug.cpp
  base64.cpp
)

set(SCRIPT_FILE
	phantomjava.sh
)
set(EXAMPLES
	examples
)
set(SCRIPT_DIR
	scripts
)
if (OS_LINUX)
  LIST(APPEND PHANTOMJS_SRCS
    keyevents_linux.cpp
  )
endif()



#
# Shared configuration.
#

# Target executable names.
set(CEF_TARGET "phantomjs")
if(OS_MACOSX)
  set(CEF_HELPER_TARGET "phantomjs Helper")
endif()

# Logical target used to link the libcef library.
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")

# Determine the target output directory.
# SET_CEF_TARGET_OUT_DIR()
set(CEF_TARGET_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}")

# Executable target.
add_executable(${CEF_TARGET} ${PHANTOMJS_SRCS})
add_dependencies(${CEF_TARGET} libcef_dll_wrapper)
target_link_libraries(${CEF_TARGET}
  libcef_lib
  libcef_dll_wrapper
  ${CEF_STANDARD_LIBS}
  json11
)

if (OS_WINDOWS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -EHsc")
    if(MSVC14)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wv:18")
    endif()
    target_link_libraries(${CEF_TARGET}
      "Ws2_32.lib"
      "Imm32.lib"
      "Winmm.lib"
    )
endif()

if (OS_LINUX)
    find_package(ZLIB REQUIRED)
    find_package(PNG REQUIRED)
    find_package(JPEG REQUIRED)

    target_link_libraries(${CEF_TARGET}
      ${CMAKE_DL_LIBS}
      ${ZLIB_LIBRARIES}
      ${PNG_LIBRARIES}
      ${JPEG_LIBRARIES}
      # FIXME: cleanup
      pcre16
      icui18n icuuc
      stdc++fs
    )

endif()

if(OS_WINDOWS AND USE_SANDBOX)
  # Logical target used to link the cef_sandbox library.
  ADD_LOGICAL_TARGET("cef_sandbox_lib" "${CEF_SANDBOX_LIB_DEBUG}" "${CEF_SANDBOX_LIB_RELEASE}")
  target_link_libraries(${CEF_TARGET} cef_sandbox_lib ${CEF_SANDBOX_STANDARD_LIBS})
endif()

# Set rpath so that libraries can be placed next to the executable.
set_target_properties(${CEF_TARGET} PROPERTIES INSTALL_RPATH "$ORIGIN")
set_target_properties(${CEF_TARGET} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
set_target_properties(${CEF_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CEF_TARGET_OUT_DIR})

# Copy binary and resource files to the target output directory.
COPY_FILES("${CEF_TARGET}" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES("${CEF_TARGET}" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")

COPY_FILES("${CEF_TARGET}" "${SCRIPT_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/${SCRIPT_DIR}" "${CEF_TARGET_OUT_DIR}")
COPY_FILES("${CEF_TARGET}" "${EXAMPLES}" "${CMAKE_CURRENT_SOURCE_DIR}" "${CEF_TARGET_OUT_DIR}")

#
# Linux configuration.
#

if(OS_LINUX)
  # Set SUID permissions on the chrome-sandbox target.
  SET_LINUX_SUID_PERMISSIONS("${CEF_TARGET}" "${CEF_TARGET_OUT_DIR}/chrome-sandbox")
endif()
cmake_minimum_required(VERSION 3.28)

project(apim-cpp-server VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

set(APIM_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if (MSVC)
  set(APIM_WARNINGS /W4 /permissive-)
else()
  set(APIM_WARNINGS -Wall -Wextra -Wpedantic)
endif()

add_library(apim-sqlite3 STATIC
  third_party/sqlite/sqlite3.c
)
target_include_directories(apim-sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)
target_compile_definitions(apim-sqlite3 PUBLIC SQLITE_OMIT_LOAD_EXTENSION SQLITE_THREADSAFE=1)
target_compile_options(apim-sqlite3 PRIVATE ${APIM_WARNINGS})

add_library(apim-xxhash STATIC
  third_party/xxhash/xxhash.c
)
target_include_directories(apim-xxhash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xxhash)
target_compile_options(apim-xxhash PRIVATE ${APIM_WARNINGS})

add_library(apim-mcp STATIC
  src/core/mcp_bridge.cpp
  src/platform/http_client.cpp
)

target_include_directories(apim-mcp
  PUBLIC
    ${APIM_INCLUDE_DIRS}
)

target_compile_options(apim-mcp PUBLIC ${APIM_WARNINGS})

add_executable(apim-cpp-server
  src/core/app.cpp
  src/core/checklist_markdown.cpp
  src/core/checklist_store.cpp
  src/core/logging.cpp
  src/core/main.cpp
  src/platform/http_server.cpp
)

target_include_directories(apim-cpp-server
  PRIVATE
    ${APIM_INCLUDE_DIRS}
)

target_compile_options(apim-cpp-server PRIVATE ${APIM_WARNINGS})
target_link_libraries(apim-cpp-server PRIVATE apim-sqlite3 apim-xxhash)

if (WIN32)
  target_link_libraries(apim-cpp-server PRIVATE ws2_32)
endif()

add_executable(apim-mcp-bridge
  src/tools/apim_mcp_bridge.cpp
)

target_link_libraries(apim-mcp-bridge PRIVATE apim-mcp)
target_compile_options(apim-mcp-bridge PRIVATE ${APIM_WARNINGS})
target_include_directories(apim-mcp-bridge PRIVATE ${APIM_INCLUDE_DIRS})

add_executable(mcp-bridge-test
  tests/mcp_bridge_test.cpp
  src/core/app.cpp
  src/core/checklist_markdown.cpp
  src/core/checklist_store.cpp
  src/core/logging.cpp
  src/platform/http_server.cpp
)

target_link_libraries(mcp-bridge-test PRIVATE apim-mcp)
target_include_directories(mcp-bridge-test PRIVATE ${APIM_INCLUDE_DIRS})
target_compile_options(mcp-bridge-test PRIVATE ${APIM_WARNINGS})
target_link_libraries(mcp-bridge-test PRIVATE apim-sqlite3 apim-xxhash)

if (WIN32)
  target_link_libraries(apim-mcp-bridge PRIVATE ws2_32)
  target_link_libraries(mcp-bridge-test PRIVATE ws2_32)
endif()

add_test(NAME mcp-bridge COMMAND mcp-bridge-test)

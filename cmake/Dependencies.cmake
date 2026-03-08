include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui
  GIT_TAG v6.1.9
)
FetchContent_MakeAvailable(ftxui)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.0
)
FetchContent_MakeAvailable(googletest)

find_package(Boost REQUIRED COMPONENTS program_options)

find_program(PG_CONFIG pg_config REQUIRED)
execute_process(COMMAND ${PG_CONFIG} --libdir    OUTPUT_VARIABLE _pg_libdir     OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${PG_CONFIG} --includedir OUTPUT_VARIABLE _pg_includedir OUTPUT_STRIP_TRAILING_WHITESPACE)
find_library(PostgreSQL_LIBRARY NAMES pq HINTS "${_pg_libdir}" REQUIRED)
set(PostgreSQL_INCLUDE_DIR "${_pg_includedir}" CACHE PATH "" FORCE)
find_package(PostgreSQL REQUIRED)

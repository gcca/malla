cmake_minimum_required(VERSION 4.2.3)
project(malla LANGUAGES CXX)

include(cmake/Dependencies.cmake)

add_library(malla-storage storage/dbconn.cpp storage/pq.cpp)
target_include_directories(malla-storage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(malla-storage PUBLIC Boost::program_options PostgreSQL::PostgreSQL)

add_executable(malla malla.cc components/searchable_dropdown.cpp components/columns_dropdown.cpp components/data_table.cpp components/where_input.cpp)
target_link_libraries(malla PRIVATE malla-storage ftxui::component)

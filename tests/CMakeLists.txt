# YACReader tests

add_subdirectory(concurrent_queue_test)
add_subdirectory(compressed_archive_test)
add_subdirectory(continuous_view_model_test)
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME thumbnail_grid_static_test
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/static/thumbnail_grid_static_test.py)
endif()

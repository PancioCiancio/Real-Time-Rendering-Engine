add_subdirectory(graphics)

add_executable(Engine
        "Main.cpp"
        "FileSystem.cpp")

target_link_libraries(Engine
        PRIVATE
        graphics
        stdc++exp)

target_include_directories(Engine
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include)

target_compile_definitions(Engine
        PRIVATE
        SDL_MAIN_HANDLED)

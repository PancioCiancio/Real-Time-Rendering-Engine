add_subdirectory(graphics)

add_executable(Engine
        "Main.cpp"
        "FileSystem.cpp")

target_link_libraries(Engine
        PRIVATE
        graphics)

target_compile_definitions(Engine
        PRIVATE
        SDL_MAIN_HANDLED)

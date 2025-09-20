cmake_minimum_required(VERSION 3.28)

add_subdirectory(Renderer)

add_executable(
        Engine
        "Main.cpp"
        "FileSystem.cpp")

target_link_libraries(
        Engine
        PRIVATE
        Renderer)

target_compile_definitions(
        Engine
        PRIVATE
        SDL_MAIN_HANDLED)

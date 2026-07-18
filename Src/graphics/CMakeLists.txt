find_package(Vulkan REQUIRED)

add_library(graphics STATIC
        graphics.cpp
        vma_impl.cpp
        volk_impl.cpp)

target_include_directories(graphics
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include/graphics
        "$ENV{VULKAN_SDK}/Include"
        ${CMAKE_SOURCE_DIR}/plugins/include)

target_link_libraries(graphics
        PRIVATE
        "$ENV{VULKAN_SDK}/Lib/SDL2.lib"
        stdc++exp)


target_compile_definitions(graphics PRIVATE
        ${VK_USE_PLATFORM})

# @todo:    sdl2.dll and assimp.dll must be located in Binary/Src/ directory.

if ("${CMAKE_BUILD_TYPE}" EQUAL "Debug")
    target_link_libraries(
            Renderer
            PUBLIC
            ${CMAKE_SOURCE_DIR}/plugins/Lib/assimp/assimp-vc143-mtd.lib)

    add_custom_command(
            TARGET graphics POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/plugins/bin/assimp/assimp-vc143-mtd.dll"
            "${CMAKE_BINARY_DIR}/Src")
else ()
    target_link_libraries(graphics
            PUBLIC
            ${CMAKE_SOURCE_DIR}/plugins/libs/assimp/assimp-vc143-mt.lib)

    add_custom_command(
            TARGET graphics POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/plugins/bin/assimp/assimp-vc143-mt.dll"
            "${CMAKE_BINARY_DIR}/src")
endif ()

configure_file("${CMAKE_SOURCE_DIR}/resources/shaders/indirect_vert.spv" "${CMAKE_BINARY_DIR}/resources/shaders/indirect_vert.spv" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/resources/shaders/vert.spv" "${CMAKE_BINARY_DIR}/resources/shaders/vert.spv" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/resources/shaders/frag.spv" "${CMAKE_BINARY_DIR}/resources/shaders/frag.spv" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/resources/meshes/behemoth.fbx" "${CMAKE_BINARY_DIR}/resources/meshes/SM_Behemoth.fbx" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/resources/meshes/mega_mike_z.fbx" "${CMAKE_BINARY_DIR}/resources/meshes/mega_mike_z.fbx" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/resources/textures/mega_mike_z/T_ZMike_Green_Base_color.png" "${CMAKE_BINARY_DIR}/resources/textures/mega_mike_z/T_ZMike_Green_Base_color.png" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/resources/textures/mega_mike_z/T_ZMike_Normal_DirectX.png" "${CMAKE_BINARY_DIR}/resources/textures/mega_mike_z/T_ZMike_Normal_DirectX.png" COPYONLY)

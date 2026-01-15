cmake_minimum_required(VERSION 3.16)
project(glbox)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SOURCES
    src/main.cpp
    libs/glad/src/glad.cpp
    libs/stb/image.cpp
    libs/imgui/imgui.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/backends/imgui_impl_glfw.cpp
    libs/imgui/backends/imgui_impl_opengl3.cpp

)

set(HEADERS
    libs/ThreadPool.h
    libs/glad/include/KHR/khrplatform.h
    libs/glad/include/glad/glad.h
    libs/stb/image.h
    libs/stb/stb_image.h

)

set(DISTFILES
    .gitignore
    LICENSE
    README.md
    src/shaders/material/texture.frag
    src/shaders/material/texture.vert
    src/shaders/material/reflection.frag
    src/shaders/material/reflection.vert
    src/shaders/depth.frag
    src/shaders/depth.vert
    src/shaders/model/model.frag
    src/shaders/model/model.vert
    src/shaders/model/model_depth.frag
    src/shaders/model/model_depth.vert
    src/shaders/sky/hdri_sky.frag
    src/shaders/sky/hdri_sky.vert
    src/shaders/sky/procedural_sky.frag
    src/shaders/sky/procedural_sky.vert
    src/shaders/sky/textuured_sky.frag
    src/shaders/sky/textuured_sky.vert
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/imgui/backends
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS}
    src/glbox/Transform.h


    src/glbox/SceneObject.h
    src/glbox/Model.h
    src/glbox/Camera.h
    src/glbox/Shader.h
    src/glbox/Texture.h
    src/glbox/ProceduralSky.h
    src/glbox/TexturedSky.h
    src/glbox/HdriSky.h
    src/glbox/geometry/Geometry.h
    src/glbox/StaticMesh.h
    src/glbox/PbrMaterial.h
    src/glbox/Types.h

    #samples/old/cubes.h
    #samples/old/lights.h
    #samples/sample.h
    #samples/old/shadows.h
    #samples/old/shadows_camera.h
    src/samples/deferred.h
    src/samples/reflection.h
    src/samples/default.h
    src/samples/sky.h

    src/samples/csm.h
    src/samples/geometryshader.h
    src/samples/ecs.h

    src/samples/old/ecs_arkanoid.h
    src/samples/old/ecs_arkanoid3d.h
    src/samples/old/oop_arkanoid3d.h

    src/samples/tesselation.h

    src/samples/pbr.h
    src/samples/objects/Sphere.h
    src/glbox/physics/Raycast.h
    src/glbox/DebugDraw.h
    src/samples/objects/math.h

)

if(WIN32)
    foreach(dll ${DLL_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${dll} $<TARGET_FILE_DIR:${PROJECT_NAME}>

    )
    endforeach()
    # Windows-specific linking
    target_link_libraries(${PROJECT_NAME}
        ${CMAKE_CURRENT_BINARY_DIR}/../../libs/glfw/glfw3dll.lib
        ${CMAKE_CURRENT_BINARY_DIR}/../../libs/assimp/lib/assimp-vc143-mt.lib
        ${CMAKE_CURRENT_BINARY_DIR}/../../libs/irrklang/irrKlang.lib
        opengl32
        glu32
    )

    # DLLs to copy
    set(DLL_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/glfw/glfw3.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/irrklang/ikpMP3.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/irrklang/irrKlang.dll
    )

else() # Unix/Linux/Mac
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(assimp REQUIRED)

    target_link_libraries(${PROJECT_NAME}
        glfw
        ${OPENGL_LIBRARIES}
        ${ASSIMP_LIBRARIES}
        # IrrKlang is Windows only; remove or handle separately for Unix
    )
endif()

# Copy shader files to output
foreach(file IN LISTS DISTFILES)
    configure_file(${file} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${file} COPYONLY)
endforeach()

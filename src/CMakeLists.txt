cmake_minimum_required(VERSION 3.16)
project(gl-box)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Sources
set(SOURCES
    main.cpp

    lib/glad/src/glad.cpp
    lib/stb/image.cpp
    lib/imgui/imgui.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_widgets.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/backends/imgui_impl_glfw.cpp
    lib/imgui/backends/imgui_impl_opengl3.cpp

)

# Headers
set(HEADERS

    lib/ThreadPool.h
    lib/glad/include/KHR/khrplatform.h
    lib/glad/include/glad/glad.h
    lib/stb/image.h
    lib/stb/stb_image.h

)

# Shader and resource files
set(DISTFILES
    ../.gitignore
    ../LICENSE
    ../README.md
    shaders/basic_texture_shader.frag
    shaders/basic_texture_shader.vert
    shaders/depth.frag
    shaders/depth.vert
    shaders/model.frag
    shaders/model.vert
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/imgui/backends
)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS}
    samples/cubes.h
    samples/lights.h
    samples/sample.h
    samples/shadows.h
    samples/shadows_camera.h

    glbox/camera.h
    glbox/Transform.h
    glbox/Material.h
    glbox/StaticMesh.h
    glbox/SceneObject.h
    glbox/model.h glbox/model.cpp


)

# Platform-specific settings
if(WIN32)
    foreach(dll ${DLL_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${dll} $<TARGET_FILE_DIR:${PROJECT_NAME}>

    )
    endforeach()
    # Windows-specific linking
    target_link_libraries(${PROJECT_NAME}
        ${CMAKE_CURRENT_BINARY_DIR}/../../../libs/glfw/glfw3dll.lib
        ${CMAKE_CURRENT_BINARY_DIR}/../../../libs/assimp/lib/assimp-vc140-mt.lib
        ${CMAKE_CURRENT_BINARY_DIR}/../../../libs/irrklang/irrKlang.lib
        opengl32
        glu32
    )

    # DLLs to copy
    set(DLL_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/glfw/glfw3.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/irrklang/ikpMP3.dll
        ${CMAKE_CURRENT_SOURCE_DIR}/../libs/irrklang/irrKlang.dll
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

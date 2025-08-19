CONFIG += console
CONFIG -= app_bundle

TARGET = gl-box
TEMPLATE = app

SOURCES += main.cpp \
    bullet_store.cpp \
    enemy_spawner.cpp \
    lib/glad/src/glad.cpp \
    lib/stb/image.cpp \
    model.cpp \
    player_mesh.cpp \
    player_model.cpp \
    shader.cpp

INCLUDEPATH += $$PWD/../include/
INCLUDEPATH +=  -L$$OUT_PWD/../include

LIBS += -L$$OUT_PWD/../libs/glfw/ -lglfw3dll
LIBS += -L$$OUT_PWD/../libs/assimp/lib/ -lassimp-vc140-mt
LIBS += -L$$OUT_PWD/../libs/irrklang/ -lirrKlang

LIBS += -lopengl32
LIBS += -lglu32

HEADERS += \
    aabb.h \
    bullet_store.h \
    capsule.h \
    enemy.h \
    enemy_spawner.h \
    geom.h \
    lib/ThreadPool.h \
    lib/glad/include/KHR/khrplatform.h \
    lib/glad/include/glad/glad.h \
    lib/stb/image.h \
    lib/stb/stb_image.h \
    model.h \
    player_mesh.h \
    player_model.h \
    shader.h \
    spritesheet.h \
    texture.h \
    vertex.h

DISTFILES += \
    ../.gitignore \
    ../LICENSE \
    ../README.md \
    shaders/basic_texture_shader.frag \
    shaders/basic_texture_shader.vert \
    shaders/basicer_shader.frag \
    shaders/basicer_shader.vert \
    shaders/blur_shader.frag \
    shaders/depth_shader.frag \
    shaders/depth_shader.vert \
    shaders/floor_shader.frag \
    shaders/geom_shader.vert \
    shaders/geom_shader2.vert \
    shaders/instanced_texture_shader.vert \
    shaders/player_shader.frag \
    shaders/player_shader.vert \
    shaders/redshader.frag \
    shaders/redshader.vert \
    shaders/sprite_shader.frag \
    shaders/texture_merge_shader.frag \
    shaders/texture_shader.frag \
    shaders/wiggly_shader.vert

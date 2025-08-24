CONFIG += console
CONFIG -= app_bundle

TARGET = gl-box
TEMPLATE = app

SOURCES += main.cpp \
   
    lib/glad/src/glad.cpp \
    lib/stb/image.cpp \
   

INCLUDEPATH += $$PWD/../include/
INCLUDEPATH +=  -L$$OUT_PWD/../include

LIBS += -L$$OUT_PWD/../libs/glfw/ -lglfw3dll
LIBS += -L$$OUT_PWD/../libs/assimp/lib/ -lassimp-vc140-mt
LIBS += -L$$OUT_PWD/../libs/irrklang/ -lirrKlang

LIBS += -lopengl32
LIBS += -lglu32

HEADERS += \
    
    lib/ThreadPool.h \
    lib/glad/include/KHR/khrplatform.h \
    lib/glad/include/glad/glad.h \
    lib/stb/image.h \
    lib/stb/stb_image.h \
   

DISTFILES += \
    ../.gitignore \
    ../LICENSE \
    ../README.md \
    shaders/basic_texture_shader.frag \
    shaders/basic_texture_shader.vert \
   
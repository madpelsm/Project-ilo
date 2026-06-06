cmake_minimum_required(VERSION 3.10)
project(Ilo)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake/Modules/)

if (NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=unused-variable")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()


if (WIN32)
    # Please don't define min() and max() macros, Windows
    add_definitions(-DNOMINMAX)
endif()

file(GLOB ILO_SRC_FILES CONFIGURE_DEPENDS "./src/*.cpp")
set(ILO_SRC_FILES ${ILO_SRC_FILES} src/tiny_obj_loader.cc)

if (EMSCRIPTEN)
    # ---- WebAssembly / WebGL2 build target (build with `emcmake cmake`) ----
    # GLM is header-only; find it on the host, bypassing the toolchain sysroot.
    find_path(GLM_INCLUDE_DIR glm/glm.hpp PATHS /usr/include /usr/local/include NO_CMAKE_FIND_ROOT_PATH)
    # Copy GLM into the build tree so it can be on the include path WITHOUT adding
    # /usr/include, which would otherwise shadow the toolchain's own libc headers.
    file(COPY ${GLM_INCLUDE_DIR}/glm DESTINATION ${CMAKE_BINARY_DIR}/glm-include)

    add_executable(ilo ${ILO_SRC_FILES})
    target_include_directories(ilo PRIVATE ${CMAKE_BINARY_DIR}/glm-include)
    target_compile_options(ilo PRIVATE "SHELL:-sUSE_SDL=2")

    set(EM_LINK "-sUSE_SDL=2 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sFULL_ES3=1 -sALLOW_MEMORY_GROWTH=1 -sGL_ENABLE_GET_PROC_ADDRESS")
    set(EM_LINK "${EM_LINK} --preload-file ${CMAKE_SOURCE_DIR}/shaders@/shaders")
    set(EM_LINK "${EM_LINK} --preload-file ${CMAKE_SOURCE_DIR}/shapes@/shapes")
    set(EM_LINK "${EM_LINK} --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html")
    set_target_properties(ilo PROPERTIES SUFFIX ".html" LINK_FLAGS "${EM_LINK}")
else()
    # ---- Native desktop build target ----
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED)
    find_package(SDL2 REQUIRED)
    find_package(GLM REQUIRED)

    include_directories(glad/include/)
    include_directories(${SDL2_INCLUDE_DIR})
    include_directories(${GLM_INCLUDE_DIR})

    add_executable(ilo ${ILO_SRC_FILES} glad/src/glad.c)
    target_link_libraries(ilo ${OPENGL_LIBRARIES} ${SDL2_LIBRARY} ${CMAKE_DL_LIBS})
endif()

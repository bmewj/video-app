cmake_minimum_required(VERSION 3.14)
project(video-app
        VERSION 1.0.0
        LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 14)

add_subdirectory(lib/glfw)
add_subdirectory(lib/FFmpeg)

# add_compile_definitions(
#     -DGL_SILENCE_DEPRECATION
# )

list(APPEND SOURCES
    src/main.cpp
    src/video_reader.cpp
    src/video_reader.hpp
)

add_executable(${PROJECT_NAME} MACOSX_BUNDLE WIN32 ${SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES 
    LINKER_LANGUAGE CXX
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"

    DEFINITIONS -stdlib=libc++

    SOVERSION   ${PROJECT_VERSION_MAJOR}
    VERSION     ${PROJECT_VERSION}
)

if(APPLE)
    list(APPEND EXTRA_LIBS
        "-framework OpenGL"
    )

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/MacOSXBundleInfo.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/assets/MacOSXBundleInfo.plist
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/assets/MacOSXBundleInfo.plist
    )

elseif(WIN32)
    list(APPEND EXTRA_LIBS
        "-lglu32 -lopengl32"
    )
    set(CMAKE_EXE_LINKER_FLAGS "-std=gnu99 -static -static-libgcc -static-libstdc++ -mwindows")
else()
    list(APPEND EXTRA_LIBS
        "-lGL -lGLU -lX11"
    )
endif()

target_link_libraries(${PROJECT_NAME}
  FFmpeg
  glfw
  ${EXTRA_LIBS}
)

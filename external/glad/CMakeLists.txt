# GLAD CMakeLists.txt
# You need to generate GLAD files from https://glad.dav1d.de/
# Select OpenGL 3.3 Core profile and generate
# Place the generated files in external/glad/

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c")
    message(FATAL_ERROR "GLAD files not found. Please generate GLAD from https://glad.dav1d.de/ and place files in external/glad/")
endif()

add_library(glad STATIC
    src/glad.c
)

target_include_directories(glad PUBLIC
    include
)


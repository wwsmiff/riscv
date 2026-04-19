cmake_minimum_required(VERSION 4.2)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(riscv-dbg)
add_executable(${PROJECT_NAME})

include(FetchContent)

FetchContent_Declare(
	glfw
	GIT_REPOSITORY https://github.com/glfw/glfw
	GIT_TAG 7b6aead
)

FetchContent_Declare(
	imgui
	GIT_REPOSITORY https://github.com/ocornut/imgui
	GIT_TAG dac0719
)

FetchContent_Declare(
	pfd
	GIT_REPOSITORY https://github.com/wwsmiff/portable-file-dialogs
	GIT_TAG 56254c6
)

FetchContent_MakeAvailable(glfw)
FetchContent_MakeAvailable(imgui)
FetchContent_MakeAvailable(pfd)

file(GLOB imgui_sources "${imgui_SOURCE_DIR}/*.cpp"
	"${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
	"${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
)

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/../riscv")
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/ext/")
target_include_directories(${PROJECT_NAME} PRIVATE "${glfw_SOURCE_DIR}/include")
target_include_directories(${PROJECT_NAME} PRIVATE "${imgui_SOURCE_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "${imgui_SOURCE_DIR}/backends")
target_include_directories(${PROJECT_NAME} PRIVATE "${pfd_SOURCE_DIR}")

target_link_libraries(${PROJECT_NAME} PRIVATE GL glfw)

target_sources(${PROJECT_NAME} PRIVATE
	main.cpp
	ext/glad.c
	"${CMAKE_SOURCE_DIR}/../riscv/core.cpp"
	"${CMAKE_SOURCE_DIR}/../riscv/disassemble.cpp"
	${imgui_sources}
)

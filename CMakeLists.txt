cmake_minimum_required(VERSION 3.22...3.31)
project(vk_raytracing_tutorial VERSION 2.0 LANGUAGES C CXX)

# Here we can set the Slang version to use (uncomment to override the default)
# set(Slang_VERSION "2025.20" CACHE STRING "Version of Slang to use")

# Add the cmake folder to the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(NvproCore2 REQUIRED)

# Include the RtTutorial common functions
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/RtTutorial.cmake")

set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")
set(TUTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/raytrace_tutorial")



# Add common library first as other samples might depend on it
add_subdirectory(common)

add_subdirectory(g_code_optimizer2)

# Make Visual Studio use this project as the startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT g_code_optimizer2)

# Install the resources and nvshaders folders
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources" DESTINATION ${CMAKE_INSTALL_PREFIX})
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/common/shaders" DESTINATION ${CMAKE_INSTALL_PREFIX}/common)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/common/io_gltf.h" DESTINATION ${CMAKE_INSTALL_PREFIX}/common)
install(DIRECTORY "${NVSHADERS_DIR}/nvshaders" DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ${Slang_GLSLANG} DESTINATION ${CMAKE_INSTALL_PREFIX})



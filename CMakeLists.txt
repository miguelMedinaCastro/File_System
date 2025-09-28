# Start of root CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

# Setting the project name based on the root folder name
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Common build flags
set(CMAKE_C_FLAGS         "-Wall -Wextra -std=c11")

# Individual build type flags
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -O2")
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS} -O0 -g3")
set(CMAKE_C_FLAGS_TEST    "${CMAKE_C_FLAGS} -O0 -g3 -D__test_build__ --coverage")

# List of components
set(COMPONENTS 
   
)

set(SOURCES 
    main.c
        src/init_file_system.c
        src/fat_fs.c
)

# file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.c")
#add_executable(${PROJECT_NAME} ${SOURCES})

# Onde salvar o executável final
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

# Cria o executável
add_executable(${PROJECT_NAME} ${SOURCES})

# Add component subdirectories using loop
foreach(COMPONENT ${COMPONENTS})
    add_subdirectory(components/${COMPONENT})
endforeach()

# Linking to coverage report tool in case of test build
if(CMAKE_BUILD_TYPE MATCHES Test)
    target_link_libraries(${PROJECT_NAME} gcov)
endif()

# Printing the size of build after building
add_custom_command(TARGET ${PROJECT_NAME} 
    POST_BUILD COMMAND size $<TARGET_FILE:${PROJECT_NAME}>)


target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

# End of root CMakeLists.txt

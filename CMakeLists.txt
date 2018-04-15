cmake_minimum_required(VERSION 3.5)
project(opengl)

set(CMAKE_CXX_STANDARD 11)
set(DEBUG_BUILD TRUE)
# Setting BUILD_SERVER makes the gui run in a separate process
# this is useful when developing plugins, because if a crash occurs,
# the main application won't crash
# set(BUILD_SERVER TRUE)

if(BUILD_SERVER)
    add_definitions(-DBUILD_SERVER)
    # DONT_FORK can be set to make the application listen for a server,
    # but it won't fork/exec by itself. This is useful when debugging the server
    # add_definitions(-DDONT_FORK)
endif()

# This is just for convenience, and is only used to not close dynamically loaded
# libraries when ran under valgrind.
# This could be made more robust, but is fine for now
if(EXISTS "/usr/include/valgrind/valgrind.h")
    add_definitions(-DVALGRIND)
endif()


if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic")
    if(DEBUG_BUILD)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
        add_definitions(-DNDEBUG)
    endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_LIST_DIR}/bin)

find_package(Freetype REQUIRED)
find_package(Lua51 REQUIRED)
find_package(SDL2 REQUIRED)
set(SDL2_LIBRARIES "-L${SDL2_LIBDIR} -lSDL2")

include_directories(${SDL2_INCLUDE_DIRS} ${LUA_INCLUDE_DIR} ${FREETYPE_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/libs)

if(BUILD_SERVER)
    set(GUI_SOURCE_FILES 
            libs/gl3w.c
            libs/gl3w.h
            libs/glcorearb.h
            src/main.cpp
            src/timer.h
            src/timer.cpp
            src/gui/server/netimpl.h
            src/gui/server/netimpl.cpp
            src/gui/glimpl.h
            src/gui/glimpl.cpp
            src/gui/luahelpers.h
            src/gui/luahelpers.cpp
            src/gui/lib.h
            src/gui/lib.cpp)
else()
    set(GUI_SOURCE_FILES 
            libs/gl3w.c
            libs/gl3w.h
            libs/glcorearb.h
            src/main.cpp
            src/timer.h
            src/timer.cpp
            src/gui/gui.h
            src/gui/gui.cpp
            src/gui/glimpl.h
            src/gui/glimpl.cpp
            src/gui/luahelpers.h
            src/gui/luahelpers.cpp
            src/gui/lib.h
            src/gui/lib.cpp)
endif()
add_executable(opengl ${GUI_SOURCE_FILES})

target_link_libraries(opengl GLEW GL X11 pthread stdc++fs dl IL ${FREETYPE_LIBRARIES} ${LUA_LIBRARIES} ${SDL2_LIBRARIES})

if(BUILD_SERVER)
    project(server)
    set(SERVER_SOURCE_FILES
            src/gui/server/guiserver.cpp
            src/timer.h
            src/timer.cpp
            src/gui/gui.h
            src/gui/gui.cpp
            src/gui/lib.h
            src/gui/lib.cpp
            src/gui/luahelpers.h
            src/gui/luahelpers.cpp)

    add_executable(server ${SERVER_SOURCE_FILES} ${LUA_INCLUDE_DIR})
    target_link_libraries(server rt dl ${LUA_LIBRARIES} ${FREETYPE_LIBRARIES})
endif()

# All the extensions
set(EXTENSION_SOURCE_SHARED
        src/gui/lib.h
        src/gui/lib.cpp
        src/gui/luahelpers.h
        src/gui/luahelpers.cpp)

project(extensions)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(LAYOUTS_DIR src/gui/extensions/layouts)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)
# Layouts
add_library(linear SHARED ${EXTENSION_SOURCE_SHARED} ${LAYOUTS_DIR}/linear.cpp)
add_library(floating SHARED ${EXTENSION_SOURCE_SHARED} ${LAYOUTS_DIR}/floating.cpp)
add_library(scroll SHARED ${EXTENSION_SOURCE_SHARED} ${LAYOUTS_DIR}/scroll.cpp)
add_library(placeholder SHARED ${EXTENSION_SOURCE_SHARED} ${LAYOUTS_DIR}/placeholder.cpp)
# Widgets
set(WIDGETS_DIR src/gui/extensions/widgets)
add_library(color SHARED ${EXTENSION_SOURCE_SHARED} ${WIDGETS_DIR}/color.cpp)
add_library(text SHARED ${EXTENSION_SOURCE_SHARED} ${WIDGETS_DIR}/text.cpp)
add_library(button SHARED ${EXTENSION_SOURCE_SHARED} ${WIDGETS_DIR}/button.cpp)
add_library(dropdown SHARED ${EXTENSION_SOURCE_SHARED} ${WIDGETS_DIR}/dropdown.cpp)
add_library(slider SHARED ${EXTENSION_SOURCE_SHARED} ${WIDGETS_DIR}/slider.cpp)
add_library(outline SHARED ${EXTENSION_SOURCE_SHARED} ${WIDGETS_DIR}/outline.cpp)
add_library(checkbox SHARED ${EXTENSION_SOURCE_SHARED} ${WIDGETS_DIR}/checkbox.cpp)
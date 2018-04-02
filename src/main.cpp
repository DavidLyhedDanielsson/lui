#include <chrono>
#include <thread>
#include <set>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <iostream>

#include "timer.h"

#include <gl3w.h>
#include <lua5.1/lua.hpp>
#include <SDL.h>

#include <gui/glimpl.h>

static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize)
{
    int* counter = (int*)ud;
    *counter += (nsize - osize);
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    else
        return realloc(ptr, nsize);
}
int luaMemoryUsage = 0;

void RegisterExtensions()
{
    GLGUI::RegisterSharedLibrary("linear", "./bin/liblinear.so");
    GLGUI::RegisterSharedLibrary("floating", "./bin/libfloating.so");
    GLGUI::RegisterSharedLibrary("color", "./bin/libcolor.so");
    GLGUI::RegisterSharedLibrary("text", "./bin/libtext.so");
    GLGUI::RegisterSharedLibrary("button", "./bin/libbutton.so");
    GLGUI::RegisterSharedLibrary("dropdown", "./bin/libdropdown.so");
    GLGUI::RegisterSharedLibrary("slider", "./bin/libslider.so");
    GLGUI::RegisterSharedLibrary("outline", "./bin/liboutline.so");
    GLGUI::RegisterSharedLibrary("scroll", "./bin/libscroll.so");
    GLGUI::RegisterSharedLibrary("checkbox", "./bin/libcheckbox.so");
}

int main(int argc, char* argv[])
{
    int resolutionX = 640;
    int resolutionY = 480;

    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Couldn't start SDL" << std::endl;
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_Window* window = SDL_CreateWindow("LUI"
                                            , SDL_WINDOWPOS_UNDEFINED
                                            , SDL_WINDOWPOS_UNDEFINED
                                            , resolutionX
                                            , resolutionY
                                            , SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if(!window) {
        std::cerr << "Couldn't create SDL window" << std::endl;
        return 1;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if(gl3wInit()) {
        std::cerr << "Couldn't initialize OpenGL" << std::endl;
        return 1;
    }

    GLGUI::InitGUI(resolutionX, resolutionY);
    
    #ifndef BUILD_SERVER
    lua_State* luaState = lua_newstate(&l_alloc, &luaMemoryUsage);
    if(luaState == nullptr)
        return 2;
    luaL_openlibs(luaState);
    #else
    lua_State* luaState = nullptr;
    #endif
    RegisterExtensions();
    GLGUI::BuildGUI(luaState, "content/lua/example.lua");

    Timer timer;
    timer.UpdateDelta();

    bool run = true;
    while(run)
    {
        timer.UpdateDelta();
        GLGUI::ReloadGUI(luaState);

        int mouseX;
        int mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        Timer guiTimer;
        guiTimer.Start();
        GLGUI::UpdateGUI(luaState, mouseX, mouseY);
        guiTimer.Stop();
        //std::cout << "Update: " << guiTimer.GetTimeMillisecondsFraction() << std::endl;
        guiTimer.Reset();

        SDL_Event event;
        while(SDL_PollEvent(&event) != 0) {
            switch(event.type) {
                case SDL_QUIT:
                    run = false;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    GLGUI::MouseDown(luaState, mouseX, mouseY);
                    break;
                case SDL_MOUSEBUTTONUP:
                    GLGUI::MouseUp(luaState, mouseX, mouseY);
                    break;
                case SDL_MOUSEWHEEL:
                    GLGUI::Scroll(luaState, mouseX, mouseY, (int32_t)event.wheel.x * 20, (int32_t)event.wheel.y * 20);
                    break;
                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym) {
                        case SDLK_r:{
                            SDL_Keymod mod = SDL_GetModState();
                            if((mod & ~KMOD_NUM) == KMOD_NONE)
                                GLGUI::BuildGUI(luaState, "content/lua/example.lua");
                            else if((mod & KMOD_SHIFT) == KMOD_SHIFT) {
                                GLGUI::InitGUI(resolutionX, resolutionY);
                                RegisterExtensions();
                                GLGUI::BuildGUI(luaState, "content/lua/example.lua");
                            }
                            break;}
                    }
                    break;
                case SDL_WINDOWEVENT:
                    switch(event.window.event)
                    {
                        case SDL_WINDOWEVENT_RESIZED:
                            resolutionX = event.window.data1;
                            resolutionY = event.window.data2;
                            glViewport(0, 0, resolutionX, resolutionY);
                            GLGUI::ResolutionChanged(luaState, resolutionX, resolutionY);
                            break;
                    }
                    break;
            }
        }

        glClearColor(0.2f, 0.2f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        guiTimer.Start();
        GLGUI::DrawGUI();
        guiTimer.Stop();
        //std::cout << "Draw: " << guiTimer.GetTimeMillisecondsFraction() << std::endl;
        guiTimer.Reset();


        SDL_GL_SwapWindow(window);

        timer.UpdateDelta();
        auto time = timer.GetDelta();
        if(time.count() < 33333333) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(33333333 - time.count()));
        }
    }

    GLGUI::DestroyGUI(luaState);
    if(luaState != nullptr)
        lua_close(luaState);

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

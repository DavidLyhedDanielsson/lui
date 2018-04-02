/*
 * A implementation of the GUI which starts up a server in a different process
 * and forwards any commands to that server.
 * 
 * If the server crashes, a single attempt will be made to start the server
 * again. If this attempt fails, it is possible to manually restar the server
 * by calling BuildGUI.
 * 
 * This is a drop-in replacement for the GL implementation, and is really only
 * meant to be used for development.
*/
#ifndef NETWORKIMPL_H__
#define NETWORKIMPL_H__

#include <lua5.1/lua.hpp>
#include <stdint.h>

#include "../gui.h"

namespace NetGUI 
{
    bool InitGUI(size_t resolutionX, size_t resolutionY);
    void BuildGUI(lua_State* state, const char* path);
    void ReloadGUI(lua_State* state);
    void DestroyGUI(lua_State* state);
    void ResolutionChanged(lua_State* state, int32_t width, int32_t height);

    void UpdateGUI(lua_State* state, int32_t x, int32_t y);
    
    void RegisterSharedLibrary(const char* name, const char* path);

    void MouseDown(lua_State* state, int32_t x, int32_t y);
    void MouseUp(lua_State* state, int32_t x, int32_t y);
    void Scroll(lua_State* state, int32_t mouseX, int32_t mouseY, int32_t scrollX, int32_t scrollY);

    const uint8_t* GetFontTextureData();
    int32_t GetFontTextureWidth();
    int32_t GetFontTextureHeight();

    int32_t GetDrawListCount();
    const GUI::DrawList* GetDrawLists();
}


#endif
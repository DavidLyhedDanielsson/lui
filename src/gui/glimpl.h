#ifndef GLIMPL_H__
#define GLIMPL_H__

#include <gl3w.h>
#include <lua5.1/lua.hpp>

#include "gui.h"

namespace GLGUI
{
    void InitGUI(size_t resolutionX, size_t resolutionY);
    void BuildGUI(lua_State* state, const char* path);
    void ReloadGUI(lua_State* state);
    void DrawGUI();
    void DestroyGUI(lua_State* state);
    void ResolutionChanged(lua_State* state, int32_t width, int32_t height);

    void UpdateGUI(lua_State* state, uint32_t x, uint32_t y);

    void MouseDown(lua_State* state, int32_t x, int32_t y);
    void MouseUp(lua_State* state, int32_t x, int32_t y);
    void Scroll(lua_State* state, int32_t mouseX, int32_t mouseY, int32_t scrollX, int32_t scrollY);

    void RegisterSharedLibrary(const char* name, const char* path);
}

#endif
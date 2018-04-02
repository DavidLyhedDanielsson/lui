#ifndef GUI_H__
#define GUI_H__

#include "guielement.h"

#include <lua5.1/lua.hpp>
#include <stdint.h>
#include <vector>

namespace GUI 
{
    struct Character {
        float uMin;
        float uMax;
        float vMin;
        float vMax;
        int16_t width;
        int16_t height;
        int8_t xOffset;
        int8_t yOffset;
        int8_t xAdvance;
    };

    struct DrawList {
        Vertex* vertices;
        int32_t vertexCount;
        uint32_t* indicies;
        int32_t indexCount;
        int32_t textureIndex;
        Rect clipRect;
    };

    void InitGUI(size_t resolutionX, size_t resolutionY);
    void BuildGUI(lua_State* state, const char* path);
    void ReloadGUI(lua_State* state);
    void DestroyGUI(lua_State* state, bool keepExtensions = false);
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
    const DrawList* GetDrawLists();
}

#endif
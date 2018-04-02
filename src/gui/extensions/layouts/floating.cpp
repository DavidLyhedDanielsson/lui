/*
 * Places an element at the given location
 * 
 * Attributes:
 * area - Where the element should be placed. Order is { x, y, width, height }
 * element - Which element should be placed there
 * 
 * Example:
 * {
 *     type = "floating"
 *     , {
 *         area = { 0, 0, 800, 200 }
 *         , element = {
 *             ...
 *         }
 *     }
 *     , {
 *         area = { 100, 100, 100, 100 }
 *         , element = {
 *             ...
 *         }
 *     }
 * }
*/

#include <stdint.h>
#include <algorithm>
#include <lua5.1/lua.hpp>
#include <gui/guielement.h>
#include <gui/lib.h>
#include <gui/luahelpers.h>

using namespace GUI;

const InitFunctions* functions;

struct Data
{
    int32_t childCount;
    Rect* childBounds;
};

extern "C"
{
    bool IsLayout() { return true; }

    void Init(int fontHeight, const InitFunctions* functions)
    {
        ::functions = functions;
    }

    int Count(lua_State* state)
    {
        int count = 0;

        int32_t childCount = lua_objlen(state, -1);
        for(int i = 0; i < childCount; ++i) {
            lua_rawgeti(state, -1, i + 1);
            if(FieldExists(state, "element")) {
                count += functions->count(state);
                lua_pop(state, 1);
            }
            lua_pop(state, 1);
        }

        return count;
    }

    int ParseLayout(lua_State* state, Layout* layout, Widget* elements, int defaults)
    {
        Data data;

        int offset = 0;
        data.childCount = lua_objlen(state, -1);
        data.childBounds = (Rect*)functions->memalloc(sizeof(Rect) * data.childCount);
        for(int32_t i = 0; i < data.childCount; ++i) {
            lua_rawgeti(state, -1, i + 1);
            Rect childBounds = { 0.0f, 0.0f, -1.0f, -1.0f};
            if(FieldExists(state, "area")) {
                // TODO: Error handling, count should be 4 and they should all be numbers
                lua_rawgeti(state, -1, 1);
                childBounds.x = lua_tonumber(state, -1);
                lua_pop(state, 1);
                lua_rawgeti(state, -1, 2);
                childBounds.y = lua_tonumber(state, -1);
                lua_pop(state, 1);
                lua_rawgeti(state, -1, 3);
                childBounds.width = lua_tonumber(state, -1);
                lua_pop(state, 1);
                lua_rawgeti(state, -1, 4);
                childBounds.height = lua_tonumber(state, -1);
                lua_pop(state, 2);
            }

            if(FieldExists(state, "element")) {
                if(childBounds.width == -1.0f || childBounds.height == -1.0f) {
                    int32_t childWidth = 0;
                    int32_t childHeight = 0;
                    functions->measure(state, &childWidth, &childHeight);

                    if(childBounds.width == -1.0f) {
                        if(childWidth != -1) {
                            childBounds.width = childWidth;
                        }
                    }
                    if(childBounds.height == -1.0f) {
                        if(childHeight != -1) {
                            childBounds.height = childHeight;
                        }
                    }
                }

                data.childBounds[i] = childBounds;
                offset += functions->parse(state, &elements[offset]);
                lua_pop(state, 1);
            }

            lua_pop(state, 1);
        }

        layout->data = (Data*)functions->memalloc(sizeof(Data));
        *(Data*)layout->data = data;
        return offset;
    }

    void BuildLayout(Layout* layout, Element** elements, int32_t elementCount)
    {
        Data data = *(Data*)layout->data;
        for(int32_t i = 0; i < data.childCount; ++i) {
            if(data.childBounds[i].width == -1)
                data.childBounds[i].width = layout->bounds.width;
            if(data.childBounds[i].height == -1)
                data.childBounds[i].height = layout->bounds.height;

            elements[i]->bounds = data.childBounds[i];
        }
    }

    void Destroy(void* data, lua_State* state)
    {
        functions->memdealloc(((Data*)data)->childBounds);
    }
}

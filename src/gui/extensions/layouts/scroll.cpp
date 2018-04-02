/*
 * Places elements in a scrolling area by creating a clip rect around it. A
 * scrollbar is automatically added if the height of all contained elements ends
 * up being greater than the layout's area's height.
 * 
 * This layout should only contain a single child.
 * 
 * Attributes:
 * max_height - if the children's combined height exceeds this value, a
 *     scrollbar is added
 * 
 * Example:
 * {
 *     type = "scrolling"
 *     , max_height = 60
 *     , {
 *         ...
 *     }
 * }
*/

// TODO: Allow only one child

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
    int32_t maxWidth;
    int32_t maxHeight;
    int32_t width;
    int32_t height;

    int32_t scrollbarOffset;

    Widget* scrollbar;
    float offset;
};

extern "C"
{
    bool IsLayout() { return true; }

    void Init(int fontHeight, const InitFunctions* functions)
    {
        ::functions = functions;
    }

    void Measure(lua_State* state, int32_t* width, int32_t* height)
    {
        int32_t maxHeight = (int32_t)GetOptionalNumber(state, "max_height", 9999999.0f);

        int32_t returnWidth = 0;
        int32_t returnHeight = 0;
        int32_t childCount = lua_objlen(state, -1);
        if(childCount > 0) {
            lua_rawgeti(state, -1, 1);
            functions->measure(state, &returnWidth, &returnHeight);
            lua_pop(state, 1);

            if(returnHeight > maxHeight) {
                returnHeight = maxHeight;
            }
        }

        *width = returnWidth;
        *height = returnHeight;
    }

    int Count(lua_State* state)
    {
        float maxHeight = GetOptionalNumber(state, "max_height", 9999999.0f);
        int32_t width = 0;
        int32_t height = 0;

        int returnValue = 0;
        int32_t childCount = lua_objlen(state, -1);
        if(childCount > 0) {
            lua_rawgeti(state, -1, 1);
            returnValue += functions->count(state);
            functions->measure(state, &width, &height);
            lua_pop(state, 1);
        }
        
        if(height > maxHeight)
            returnValue++;

        return returnValue;
    }

    int ParseLayout(lua_State* state, Layout* layout, Widget* elements, int defaults)
    {
        Data data;
        // TODO: Replace magic numbers
        data.maxWidth = GetOptionalNumber(state, "max_width", 9999999.0f, defaults);
        data.maxHeight = GetOptionalNumber(state, "max_height", 9999999.0f, defaults);
        data.width = 0.0f;
        data.height = 0.0f;
        data.offset = 0.0f;

        int returnValue = 0;

        if(lua_objlen(state, -1) > 0) {
            lua_rawgeti(state, -1, 1);
            returnValue = functions->parse(state, elements);
            functions->measure(state, &data.width, &data.height);
            lua_pop(state, 1);

            if(data.height > data.maxHeight) {
                lua_createtable(state, 0, 4);
                lua_pushstring(state, "type");
                lua_pushstring(state, "slider");
                lua_settable(state, -3);
                lua_pushstring(state, "value_min");
                lua_pushnumber(state, 0);
                lua_settable(state, -3);
                lua_pushstring(state, "value_max");
                lua_pushnumber(state, data.height - data.maxHeight);
                lua_settable(state, -3);
                lua_pushstring(state, "direction");
                lua_pushstring(state, "vertical");
                lua_settable(state, -3);
                functions->parse(state, elements + returnValue);
                lua_pop(state, 1);

                data.scrollbar = elements + returnValue;
                returnValue++;
            }
        }

        layout->data = functions->memalloc(sizeof(Data));
        *((Data*)layout->data) = data;
        return returnValue;
    }

    void BuildLayout(Layout* layout, Element** elements, int32_t elementCount)
    {
        Data data = *((Data*)layout->data);
        Rect bounds = layout->bounds;

        // TODO: What if elementCount == 0?
        elements[0]->bounds = bounds;
        elements[0]->bounds.y -= data.offset;

        functions->setClipRect(elements[0], bounds);

        if(data.height > data.maxHeight) {
            elements[1]->bounds = { bounds.x + bounds.width - 32.0f, bounds.y, 32.0f, bounds.height };
            functions->setClipRect(elements[1], bounds);
            functions->setNumber(data.scrollbar, "value", data.offset);
        }
    }

    bool OnChildUpdate(GUI::Layout* layout, lua_State* state, GUI::Widget* child, int32_t x, int32_t y)
    {
        Data* data = (Data*)layout->data;
        if(child == data->scrollbar) {
            functions->queryNumber(data->scrollbar, "value", &data->offset);
            functions->build(layout);
            return true;
        }
        return false;
    }

    bool OnChildReleased(GUI::Widget* element, lua_State* state, GUI::Widget* child, int32_t x, int32_t y)
    {
        return child == ((Data*)element->data)->scrollbar;
    }

    bool OnScroll(GUI::Layout* layout, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)layout->data;
        functions->setNumber(data->scrollbar, "value", data->offset - y);
        functions->queryNumber(data->scrollbar, "value", &data->offset);
        functions->build(layout);

        return true;
    }
}

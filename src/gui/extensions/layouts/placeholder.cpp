/*
 * Holds a layout inside of it. The layout to hold is specified by its name.
 * 
 * Passing "layout" to SetString, and then the name of the layout will rebuild
 * the given layout and place it within this layout's bounds
*/
#include <lua5.1/lua.hpp>
#include <gui/guielement.h>
#include <gui/lib.h>
#include <gui/luahelpers.h>
#include <gui/parseHelpers.h>

using namespace GUI;

const InitFunctions* functions;

struct Data
{
    Layout* layout;
};

void ChangeLayout(Layout* layout, Data* data, Layout* newLayout)
{
    functions->setDraw(data->layout, false, -1);
    data->layout = newLayout;
    newLayout->bounds = layout->bounds;
    functions->build(newLayout);
    functions->setDraw(newLayout, true, -1);
}

extern "C"
{
    void Init(int fontHeight, const InitFunctions* functions)
    {
        ::functions = functions;
    }

    int Count(lua_State* state)
    {
        return 0;
    }

    int ParseLayout(lua_State* state, Layout* layout, int defaults)
    {
        Data* data = (Data*)functions->memalloc(sizeof(Data));

        if(FieldExists(state, "default")) {
            data->layout = (Layout*)functions->getNamedElement(lua_tostring(state, -1));
            lua_pop(state, 1);
        } else {
            data->layout = nullptr;
        }

        layout->data = data;
        return 0;
    }

    int BuildLayout(Layout* layout, Element** children, int32_t childCount)
    {
        Data* data = (Data*)layout->data;
        if(data->layout) {
            data->layout->bounds = layout->bounds;
            ChangeLayout(layout, data, data->layout);
            functions->setDraw(data->layout, true, 1);
        }

        return 0;
    }

    bool SetString(Layout* layout, const char* key, const char* value)
    {
        Data* data = (Data*)layout->data;
        if(streq(key, "layout")) {
            Layout* newLayout = (Layout*)functions->getNamedElement(value);
            if(!newLayout)
                return false;

            ChangeLayout(layout, data, newLayout);
            return true;
        }

        return false;
    }
}

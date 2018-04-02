/*
 * A simple color
 * 
 * Attributes:
 * color
 * 
 * Example:
 * {
 *     type = "color"
 *     , color = "123456FF"
 * }
*/
#include <stdint.h>
#include <lua5.1/lua.hpp>
#include <gui/guielement.h>
#include <gui/lib.h>
#include <gui/luahelpers.h>

using namespace GUI;

const InitFunctions* functions;

struct Data
{
    Color color;
};

extern "C"
{
    void Init(int fontHeight, const InitFunctions* functions)
    {
        ::functions = functions;
    }

    int ParseWidget(lua_State* state, GUI::Widget* widget, int defaults)
    {
        Data* data = (Data*)functions->memalloc(sizeof(Data));
        data->color = GetOptionalColor(state, "color", {255, 255, 255, 255}, defaults);

        widget->data = data;

        QUAD_ALLOC(widget, 1);

        return 1;
    }

    void BuildWidget(GUI::Widget* widget)
    {
        Data* data = (Data*)widget->data;
        GUI::Rect bounds = widget->bounds;
        CreateColoredQuad(widget
                            , bounds.x
                            , bounds.y
                            , bounds.width
                            , bounds.height
                            , data->color.r
                            , data->color.g
                            , data->color.b
                            , data->color.a);
    }
}

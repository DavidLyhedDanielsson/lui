/*
 * Just some text
 * 
 * Attributes:
 * parseHelpers.h:
 * Text
 * BackgroundColor
*/
#include <stdint.h>
#include <lua5.1/lua.hpp>
#include <gui/guielement.h>
#include <gui/lib.h>
#include <gui/luahelpers.h>
#include <gui/parseHelpers.h>

using namespace GUI;

const InitFunctions* functions;
static int fontHeight;

struct Data
{
    Color backgroundColor;

    Origin textAlignment;
    Color textColor;
    char* text;
};

extern "C"
{
    void Init(int fontHeight, const InitFunctions* functions)
    {
        ::functions = functions;
        ::fontHeight = fontHeight;
    }

    void Measure(lua_State* state, int32_t* width, int32_t* height)
    {
        const char* text;
        lua_getfield(state, -1, "text");
        if(!lua_isnil(state, -1) && lua_isstring(state, -1)) {
            text = lua_tostring(state, -1);
        } else {
            text = "";
        }
        functions->measureText(text, width, height);
        *height = fontHeight;
        lua_pop(state, 1);
    }

    int ParseWidget(lua_State* state, Widget* widget, int defaults)
    {
        Data data;
        BackgroundColor::Parse(state, &data.backgroundColor, defaults);
        Text::Parse(state, functions, &data.text, &data.textColor, &data.textAlignment, defaults);

        // Text + background color
        QUAD_ALLOC(widget, strlen(data.text) + 1);
        
        widget->data = functions->memalloc(sizeof(Data));
        *(Data*)widget->data = data;
        return 1;
    }

    void BuildWidget(Widget* widget)
    {
        Data* data = (Data*)widget->data;

        BackgroundColor::Build(widget, data->backgroundColor);
        Text::Build(functions, widget, data->text, data->textColor, data->textAlignment);
    }

    void Destroy(void* data, lua_State* state)
    {
        Text::Dealloc(functions, ((Data*)data)->text);
    }
}
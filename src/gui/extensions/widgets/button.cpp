/*
 * A simple button which can send a message back to the application, or call a
 * lua function
 * 
 * Attributes:
 * on_click - the function to call, or message to send to the application
 * parseHelpers.h:
 * ClickableBackgroundColor
 * Text
 * 
 * Example:
 * {
 *     type = "button"
 *     , text = "A button"
 *     , on_click = "Hello application"
 * }
 * 
 * {
 *     type = "button"
 *     , text = "Do things"
 *     , on_click = SomeFunction
 * }
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
    char* text;
    char* message;
    int32_t luaFunctionIndex;

    bool down;

    GUI::Color color;
    GUI::Color bgcolor;
    GUI::Color bgcolorHover;
    GUI::Color bgcolorDown;

    Origin origin;
};

void ChangeColor(GUI::Widget* button, GUI::Color color) {
    for(int i = 0; i < 4; ++i) {
        button->vertices[i].r = color.r;
        button->vertices[i].g = color.g;
        button->vertices[i].b = color.b;
        button->vertices[i].a = color.a;
    }
    button->modified = true;
}

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

    void Destroy(void* widgetData, lua_State* state)
    {
        Data* data = (Data*)widgetData;
        Text::Dealloc(functions, data->text);
        functions->memdealloc(data->message);
        luaL_unref(state, LUA_REGISTRYINDEX, data->luaFunctionIndex);
    }

    int ParseWidget(lua_State* state, GUI::Widget* widget, int defaults)
    {
        Data* data = (Data*)functions->memalloc(sizeof(Data));

        data->down = false;
        data->message = nullptr;
        data->luaFunctionIndex = -1;

        ClickableBackgroundColor::Parse(state, &data->bgcolor, &data->bgcolorHover, &data->bgcolorDown, defaults);
        Text::Parse(state, functions, &data->text, &data->color, &data->origin, defaults);

        if(FieldExists(state, "on_click")) {
            if(lua_isstring(state, -1)) {
                data->message = (char*)functions->memalloc(128);
                strcpy(data->message, lua_tostring(state, -1));
                lua_pop(state, 1);
            } else {
                data->luaFunctionIndex = luaL_ref(state, LUA_REGISTRYINDEX);
            }
        }

        // Background is at vertices 0-3, then comes text
        QUAD_ALLOC(widget, strlen(data->text) + 1);

        widget->data = data;
        return 1;
    }

    void BuildWidget(GUI::Widget* widget)
    {
        Data* data = (Data*)widget->data;

        ClickableBackgroundColor::Build(widget, data->bgcolor);
        Text::Build(functions, widget, data->text, data->color, data->origin);
    }

    void OnEnter(GUI::Widget* widget, lua_State* state)
    {
        Data* data = (Data*)widget->data;
        if(!data->down)
            ChangeColor(widget, data->bgcolorHover);
    }

    void OnExit(GUI::Widget* widget, lua_State* state)
    {
        Data* data = (Data*)widget->data;
        if(!data->down)
            ChangeColor(widget, data->bgcolor);
    }

    bool OnClick(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;
        ChangeColor(widget, data->bgcolorDown);
        data->down = true;
        return true;
    }

    bool OnReleaseInside(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;

        if(data->message)
            functions->message(data->message);
        else if(data->luaFunctionIndex != -1)
        {
            lua_rawgeti(state, LUA_REGISTRYINDEX, data->luaFunctionIndex);
            lua_call(state, 0, 0);
        }

        ChangeColor(widget, data->bgcolorHover);
        data->down = false;

        return true;
    }

    bool OnReleaseOutside(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;
        ChangeColor(widget, data->bgcolor);
        data->down = false;

        return true;
    }

    int QueryString(GUI::Widget* widget, const char* key, char* value, int32_t maxLength) {
        if(streq(key, "text")) {
            Data* data = (Data*)widget->data;
            int32_t length = (int32_t)strlen(data->text);
            if(length + 1 <= maxLength) {
                memcpy(value, data->text, length + 1);
                return length;
            }
        }

        return -1;
    }
}
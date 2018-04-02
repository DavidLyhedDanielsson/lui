/*
 * A checkbox
 * 
 * Attributes:
 * checked
 * parseHelpers.h:
 * Text
 * ClickableBackgroundColor
 * 
 * Example:
 * {
 *     type = "checkbox"
 *     , text = "Some setting"
 *     , checked = true
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

    bool down;

    GUI::Color color;
    GUI::Color bgcolor;
    GUI::Color bgcolorHover;
    GUI::Color bgcolorDown;

    Origin origin;

    bool checked;
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

const static int32_t CHECK_PADDING = 8;
const static int32_t CHECK_BORDER = 2;

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
        *width += 38;
        lua_pop(state, 1);
    }

    void Destroy(void* widgetData, lua_State* state)
    {
        Data* data = (Data*)widgetData;
        Text::Dealloc(functions, data->text);
    }

    int ParseWidget(lua_State* state, GUI::Widget* widget, int defaults)
    {
        Data* data = (Data*)functions->memalloc(sizeof(Data));

        data->down = false;
        data->checked = GetOptionalBoolean(state, "checked", false, -1);

        ClickableBackgroundColor::Parse(state, &data->bgcolor, &data->bgcolorHover, &data->bgcolorDown, defaults);
        Text::Parse(state, functions, &data->text, &data->color, &data->origin, defaults);

        // Background quad 0, checkmark 1 and 2, then text
        QUAD_ALLOC(widget, strlen(data->text) + 3);

        widget->data = data;
        return 1;
    }

    void BuildWidget(GUI::Widget* widget)
    {
        Data* data = (Data*)widget->data;
        const Rect bounds = widget->bounds;

        ClickableBackgroundColor::Build(widget, data->bgcolor);
        CreateColoredQuad(widget
                            , bounds.x + CHECK_PADDING
                            , bounds.y + CHECK_PADDING
                            , bounds.height - CHECK_PADDING * 2
                            , bounds.height - CHECK_PADDING * 2
                            , COLOR_PRIMARY.r
                            , COLOR_PRIMARY.g
                            , COLOR_PRIMARY.b
                            , COLOR_PRIMARY.a);
        Color color = data->checked ? COLOR_PRIMARY : COLOR_BACKGROUND;
        CreateColoredQuad(widget
                            , bounds.x + CHECK_PADDING + CHECK_BORDER
                            , bounds.y + CHECK_PADDING + CHECK_BORDER
                            , bounds.height - (CHECK_PADDING + CHECK_BORDER) * 2
                            , bounds.height - (CHECK_PADDING + CHECK_BORDER) * 2
                            , color.r
                            , color.g
                            , color.b
                            , color.a);
        Rect textRect = { bounds.x + 38.0f, bounds.y, bounds.width - 38.0f, bounds.height };
        Text::Build(functions, widget, textRect, data->text, data->color, data->origin);
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

        data->down = false;
        data->checked = !data->checked;

        if(data->checked) {
            for(int i = 0; i < 4; ++i) {
                widget->vertices[8 + i].r = widget->vertices[4 + i].r;
                widget->vertices[8 + i].g = widget->vertices[4 + i].g;
                widget->vertices[8 + i].b = widget->vertices[4 + i].b;
                widget->vertices[8 + i].a = widget->vertices[4 + i].a;
            }
        } else {
            for(int i = 0; i < 4; ++i) {
                widget->vertices[8 + i].r = COLOR_BACKGROUND.r;
                widget->vertices[8 + i].g = COLOR_BACKGROUND.g;
                widget->vertices[8 + i].b = COLOR_BACKGROUND.b;
                widget->vertices[8 + i].a = COLOR_BACKGROUND.a;
            }

        }

        ChangeColor(widget, data->bgcolorHover);

        return true;
    }

    bool OnReleaseOutside(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;
        data->down = false;

        ChangeColor(widget, data->bgcolor);

        return true;
    }
}

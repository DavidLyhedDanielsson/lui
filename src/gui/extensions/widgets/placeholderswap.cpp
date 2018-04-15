/*
 * Given a target placeholder and a target layout, tells the placeholder to swap
 * its element fo the given layout.
 * 
 * Currently acts the same as a button
 * 
 * Example:
 * TODO
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
    char* placeholderTargetName;
    Layout* placeholderTarget;
    char* layoutName;

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
        functions->memdealloc(data->placeholderTargetName);
        functions->memdealloc(data->layoutName);
    }

    int ParseWidget(lua_State* state, GUI::Widget* widget, int defaults)
    {
        Data* data = (Data*)functions->memalloc(sizeof(Data));

        data->down = false;
        data->placeholderTarget = nullptr;

        ClickableBackgroundColor::Parse(state, &data->bgcolor, &data->bgcolorHover, &data->bgcolorDown, defaults);
        Text::Parse(state, functions, &data->text, &data->color, &data->origin, defaults);

        if(FieldExists(state, "placeholder_target")) {
            const char* target = lua_tostring(state, -1);
            data->placeholderTargetName = (char*)functions->memalloc(strlen(target) + 1);
            strcpy(data->placeholderTargetName, target);
            lua_pop(state, 1);
        } else {
            data->placeholderTargetName = nullptr;
        }

        if(FieldExists(state, "layout")) {
            const char* layout = lua_tostring(state, -1);
            data->layoutName = (char*)functions->memalloc(strlen(layout) + 1);
            strcpy(data->layoutName, layout);
            lua_pop(state, 1);
        } else {
            data->layoutName = nullptr;
        }

        // Background is at vertices 0-3, then comes text
        QUAD_ALLOC(widget, strlen(data->text) + 1);

        widget->data = data;
        return 1;
    }

    void BuildWidget(GUI::Widget* widget)
    {
        Data* data = (Data*)widget->data;

        if(data->placeholderTargetName != nullptr) {
            // TODO: Error handling, use Element::type to check
            data->placeholderTarget = (Layout*)functions->getNamedElement(data->placeholderTargetName);
        } else {
            data->placeholderTarget = nullptr;
        }

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

        ChangeColor(widget, data->bgcolorHover);
        data->down = false;

        if(data->placeholderTarget && data->layoutName)
        {
            functions->setString(data->placeholderTarget, "layout", data->layoutName);
        }

        return true;
    }

    bool OnReleaseOutside(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;
        ChangeColor(widget, data->bgcolor);
        data->down = false;

        return true;
    }
}

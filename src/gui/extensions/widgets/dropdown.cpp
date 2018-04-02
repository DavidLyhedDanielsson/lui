/*
 * A configurable dropdown menu. Can either be a traditional context menu,
 * a "settings" dropdown menu which changes text to match the setting, or a
 * dropdown menu with static text
 * 
 * Attributes:
 * dropdown_height - max height of the dropdown, if the dropdown's element's
 *     combined height is greater than this, a scrollbar is added
 * style = "context|options|dropdown"
 * open_direction = "down|right" - down places dropdown below widget, right
 *     places it to the right
 * open_on = "click|hover"
 * close_on = "click|hover" - click means the user has to click outside the
 *     popup to close it, hover means they just have to stop hovering it
 *     and its parent
 * parseHelpers.h:
 * Text
 * ClickableBackgroundColor
 * 
 * Setting a style automatically sets open_direction, open_on, and close_on
 * 
 * Example:
 * {
 *     type = "dropdown"
 *     , style = "context"
 *     , text = "Context menu"
 *     , {
 *         ...
 *     }
 *     , {
 *         ...
 *     }
 * }
*/
#include <stdint.h>
#include <lua5.1/lua.hpp>
#include <gui/guielement.h>
#include <gui/lib.h>
#include <gui/luahelpers.h>
#include <math.h>
#include <gui/parseHelpers.h>

using namespace GUI;

const InitFunctions* functions;
static int fontHeight;

enum class OPEN_ON {
    CLICK
    , HOVER
};

enum class DIRECTION {
    DOWN
    , RIGHT
};

enum class WIDTH {
    BUTTON
    , CHILDREN
};

struct Vec2
{
    int32_t width;
    int32_t height;
};

struct Data
{
    char* text;
    GUI::Color color;
    GUI::Color bgcolor;
    GUI::Color bgcolorHover;
    GUI::Color bgcolorDown;

    int32_t childCount;
    Element** children;

    OPEN_ON openOn;
    CLOSE_ON closeOn; // HOVER means as soon as the mouse exits the widget

    bool open;
    bool widgetDown;
    bool changeText;

    Rect clipRect;
    int32_t dropdownHeight;

    DIRECTION openDirection;
    WIDTH width;

    Origin origin;
};

#include <cstring>

const static int BACKGROUND_OFFSET = 0;
const static int BUTTON_OFFSET = BACKGROUND_OFFSET + 1;
const static int TEXT_OFFSET = BUTTON_OFFSET + 1;

void Open(Widget* widget)
{
    Data* data = (Data*)widget->data;
    if(!data->open) {
        functions->openPopup(data->children, 1, data->closeOn);

        widget->modified = true;
        data->open = true;
    }
}

void Close(lua_State* state, Widget* widget)
{
    Data* data = (Data*)widget->data;
    if(data->open) {
        functions->closePopup(state);
    }
}

void ChangeColor(GUI::Widget* widget, const GUI::Color& color)
{
    widget->modified = true;
    for(int i = 0; i < 4; ++i) {
        widget->vertices[i].r = color.r;
        widget->vertices[i].g = color.g;
        widget->vertices[i].b = color.b;
        widget->vertices[i].a = color.a;
    }
}

extern "C"
{
    void Init(int fontHeight, const InitFunctions* functions)
    {
        ::functions = functions;
        ::fontHeight = fontHeight;
    }

    int Count(lua_State* state)
    {
        int returnValue = 1; // The dropdown button

        // All children
        int childCount = lua_objlen(state, -1);
        for(int i = 0; i < childCount; ++i) {
            lua_rawgeti(state, -1, i + 1);
            returnValue += functions->count(state);
            lua_pop(state, 1);
        }

        // Scrollbar if dropdown_height is given
        if(FieldExists(state, "dropdown_height")) {
            returnValue++;
            lua_pop(state, 1);
        }

        return returnValue;
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
        int32_t tempWidth;
        int32_t tempHeight;
        functions->measureText(">", &tempWidth, &tempHeight);
        *width += 32.0f;
        *width += tempWidth;
        lua_pop(state, 1);
    }

    void Destroy(void* widgetData, lua_State* state)
    {
        Data* data = (Data*)widgetData;
        functions->memdealloc(data->text);
    }

    void OnEnter(GUI::Widget* widget, lua_State* state)
    {
        Data* data = (Data*)widget->data;
        ChangeColor(widget, data->bgcolorHover);
        if(data->openOn == OPEN_ON::HOVER)
            Open(widget);
    }

    void OnExit(GUI::Widget* widget, lua_State* state)
    {
        ChangeColor(widget, ((Data*)widget->data)->bgcolor);
    }

    bool OnClick(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;
        data->widgetDown = true;
        if(data->openOn != OPEN_ON::HOVER)
            Open(widget);

        return !data->open;
    }

    bool OnReleaseInside(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;
        if(!data->widgetDown)
            Close(state, widget);
        else if(!data->open)
            data->widgetDown = false;

        return false;
    }

    bool OnReleaseOutside(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;
        data->widgetDown = false;

        return false;
    }

    int ParseWidget(lua_State* state, GUI::Widget* widget, int defaults)
    {
        Data data;
        data.widgetDown = false;
        data.open = false;
        data.children = nullptr;
        data.childCount = 0;

        Text::Parse(state, functions, &data.text, &data.color, &data.origin, defaults);
        ClickableBackgroundColor::Parse(state, &data.bgcolor, &data.bgcolorHover, &data.bgcolorDown, defaults);

        char style[32] = "";
        GetOptionalString(state, "style", style, 32, nullptr, "", defaults);
        if(streq(style, "dropdown")) {
            data.openOn = OPEN_ON::CLICK;
            data.closeOn = CLOSE_ON::CLICK;
            data.openDirection = DIRECTION::DOWN;
            data.width = WIDTH::BUTTON;
            data.changeText = false;
        } else if(streq(style, "context")) {
            data.openOn = OPEN_ON::HOVER;
            data.closeOn = CLOSE_ON::HOVER;
            data.openDirection = DIRECTION::RIGHT;
            data.width = WIDTH::CHILDREN;
            data.changeText = false;
        } else if(streq(style, "options")) {
            data.openOn = OPEN_ON::CLICK;
            data.closeOn = CLOSE_ON::CLICK;
            data.openDirection = DIRECTION::DOWN;
            data.width = WIDTH::BUTTON;
            data.changeText = true;
        }

        char openDirection[32] = "";
        GetOptionalString(state, "open_direction", openDirection, 32, nullptr, "", defaults);
        if(streq(openDirection, "down")) {
            data.openDirection = DIRECTION::DOWN;
        } else if(streq(openDirection, "right")) {
            data.openDirection = DIRECTION::RIGHT;
        }

        char openOn[32] = "";
        GetOptionalString(state, "open_on", openOn, 32, nullptr, "", defaults);
        if(streq(openOn, "click")) {
            data.openOn = OPEN_ON::CLICK;
        } else if(streq(openOn, "hover")) {
            data.openOn = OPEN_ON::HOVER;
        }

        char close_on[32] = "";
        GetOptionalString(state, "close_on", close_on, 32, nullptr, "", defaults);
        if(streq(close_on, "click")) {
            data.closeOn = CLOSE_ON::CLICK;
        } else if(streq(close_on, "hover")) {
            data.closeOn = CLOSE_ON::HOVER;
        }

        // TODO: Replace with constant
        float dropdownHeight = GetOptionalNumber(state, "dropdown_height", 999999.0f, defaults);
        data.dropdownHeight = (int32_t)dropdownHeight;

        int childOffset = 1;
        int32_t childCount = lua_objlen(state, -1);
        int32_t totalHeight = 0;

        for(int i = 0; i < childCount; ++i) {
            lua_rawgeti(state, -1, i + 1);
            int32_t elementWidth = 0;
            int32_t elementHeight = 0;
            functions->measure(state, &elementWidth, &elementHeight);
            totalHeight += elementHeight;
            lua_pop(state, 1);

            if(totalHeight > dropdownHeight) {
                break;
            }
        }

        if(totalHeight > dropdownHeight) {
            lua_createtable(state, childCount, 0);
            lua_pushstring(state, "type");
            lua_pushstring(state, "scroll");
            lua_settable(state, -3);
            lua_pushstring(state, "max_height");
            lua_pushnumber(state, dropdownHeight);
            lua_settable(state, -3);

            lua_createtable(state, childCount, 0);
            lua_pushstring(state, "type");
            lua_pushstring(state, "linear");
            lua_settable(state, -3);
            lua_pushstring(state, "direction");
            lua_pushstring(state, "vertical");
            lua_settable(state, -3);

            for(int i = 0; i < childCount; ++i) {
                lua_rawgeti(state, -3, i + 1);
                lua_rawseti(state, -2, i + 1);
            }

            lua_rawseti(state, -2, 1);
        } else {
            lua_createtable(state, childCount, 0);
            for(int i = 0; i < childCount; ++i) {
                lua_rawgeti(state, -2, i + 1);
                lua_rawseti(state, -2, i + 1);
            }
            lua_pushstring(state, "type");
            lua_pushstring(state, "linear");
            lua_settable(state, -3);
            lua_pushstring(state, "direction");
            lua_pushstring(state, "vertical");
            lua_settable(state, -3);
        }

        childOffset += functions->parse(state, widget + 1);

        lua_pop(state, 1);

        // Background, button, text
        QUAD_ALLOC(widget, strlen(data.text) + 2);

        widget->data = (Data*)functions->memalloc(sizeof(Data));
        *(Data*)widget->data = data;
        return childOffset;
    }

    void BuildWidget(GUI::Widget* widget)
    {
        Data* data = (Data*)widget->data;

        ClickableBackgroundColor::Build(widget, data->bgcolor);
        Text::Build(functions, widget, ">", data->color, (Origin)((int)Origin::TOP | (int)Origin::RIGHT));

        if(data->openDirection == DIRECTION::DOWN) {

        }

        Text::Build(functions, widget, data->text, data->color, data->origin);
    }

    void BuildChildren(GUI::Widget* widget, GUI::Element** children, int32_t childCount)
    {
        Data* data = (Data*)widget->data;
        Rect bounds = widget->bounds;

        Vec2 childOffset = { 0, 0 };
        if(data->openDirection == DIRECTION::RIGHT) {
            childOffset.width = bounds.width;
        } else if(data->openDirection == DIRECTION::DOWN) {
            childOffset.height = bounds.height;
        }

        // TODO: Make sure only 1 child
        children[0]->bounds = { bounds.x + childOffset.width, bounds.y + childOffset.height, bounds.width, (float)data->dropdownHeight };
        functions->setDraw(children[0], false, -1);

        data->childCount = childCount;
        data->children = children;
        data->clipRect = children[0]->bounds;
    }

    bool OnChildReleased(GUI::Widget* widget, lua_State* state, GUI::Widget* child, int32_t x, int32_t y)
    {
        Close(state, widget);

        Data* data = (Data*)widget->data;
        if(child != data->children[data->childCount - 1]) {
            Close(state, widget);
            if(data->changeText) {
                char buffer[128];
                int length = functions->queryString(child, "text", buffer, 127);
                if(length > 0) {
                    functions->memdealloc(data->text);
                    data->text = (char*)functions->memalloc(length + 1);
                    strcpy(data->text, buffer);

                    widget->offsetData = { 0, 0, 0 };
                    functions->memdealloc(widget->vertices);
                    functions->memdealloc(widget->indicies);
                    QUAD_ALLOC(widget, strlen(data->text) + 2);

                    BuildWidget(widget);
                    widget->modified = true;
                }
            }
        }

        return true;
    }

    void OnPopupClosed(GUI::Widget* widget, lua_State* state, bool mouseDown)
    {
        Data* data = (Data*)widget->data;
        data->open = false;
        data->widgetDown = mouseDown;
        ChangeColor(widget, data->bgcolor);
    }
}

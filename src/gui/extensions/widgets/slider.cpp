/*
 * A slider
 * 
 * Attributes:
 * parseHelpers.h:
 * BackgroundColor
 * Scrollbar
 * 
 * Example:
 * {
 *     type = "slider"
 *     , direction = "horizontal"
 *     , value = 0.5
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

struct Data
{
    GUI::Color backgroundColor;
    GUI::Color thumbColor;

    // Logical units
    float currentValue;
    float minValue;
    float maxValue;
    float valueIncrement;
    // Graphical units
    float thumbSize;
    float pos;

    Scrollbar::DIRECTION direction;

    bool mouseDown;
    bool mouseInside;
};

extern "C"
{
    void Init(int fontHeight, const InitFunctions* functions)
    {
        ::functions = functions;
    }

    int ParseWidget(lua_State* state, GUI::Widget* widget, int defaults)
    {
        Data data;

        data.mouseDown = false;
        data.mouseInside = false;
        data.thumbSize = 0.0f;
        data.pos = 0.0f;

        BackgroundColor::Parse(state, &data.backgroundColor, defaults);
        Scrollbar::Parse(state
                            , &data.currentValue
                            , &data.minValue
                            , &data.maxValue
                            , &data.valueIncrement
                            , &data.direction
                            , &data.thumbColor
                            , defaults);

        QUAD_ALLOC(widget, 2);
        
        widget->data = (Data*)functions->memalloc(sizeof(Data));
        *(Data*)widget->data = data;
        return 1;
    }

    void BuildWidget(GUI::Widget* widget)
    {
        Data* data = (Data*)widget->data;
        Rect bounds = widget->bounds;

        CreateColoredQuad(widget
                            , bounds.x
                            , bounds.y
                            , bounds.width
                            , bounds.height
                            , data->backgroundColor.r
                            , data->backgroundColor.g
                            , data->backgroundColor.b
                            , data->backgroundColor.a);

        if(data->direction == Scrollbar::HORIZONTAL) {
            Scrollbar::BuildHorizontal(widget
                                        , bounds
                                        , data->currentValue
                                        , data->minValue
                                        , data->maxValue
                                        , data->valueIncrement
                                        , &data->pos
                                        , &data->thumbSize
                                        , data->thumbColor);
        } else {
            Scrollbar::BuildVertical(widget
                                        , bounds
                                        , data->currentValue
                                        , data->minValue
                                        , data->maxValue
                                        , data->valueIncrement
                                        , &data->pos
                                        , &data->thumbSize
                                        , data->thumbColor);
        }
    }

    void OnEnter(GUI::Widget* widget, lua_State* state)
    {
        ((Data*)widget->data)->mouseInside = true;
    }

    void OnExit(GUI::Widget* widget, lua_State* state)
    {
        Data* data = (Data*)widget->data;
        data->mouseInside = false;

        if(!data->mouseDown)
            widget->update = false;
    }

    void OnUpdate(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;

        if(data->direction == Scrollbar::HORIZONTAL) {
            Scrollbar::UpdateHorizontal(x
                                        , widget->bounds
                                        , data->minValue
                                        , data->maxValue
                                        , data->valueIncrement
                                        , data->thumbSize
                                        , &data->currentValue
                                        , &data->pos);

            widget->vertices[4].x = data->pos;
            widget->vertices[5].x = data->pos;
            widget->vertices[6].x = data->pos + data->thumbSize;
            widget->vertices[7].x = data->pos + data->thumbSize;
        } else {
            Scrollbar::UpdateVertical(y
                                        , widget->bounds
                                        , data->minValue
                                        , data->maxValue
                                        , data->valueIncrement
                                        , data->thumbSize
                                        , &data->currentValue
                                        , &data->pos);

            widget->vertices[4].y = data->pos;
            widget->vertices[5].y = data->pos + data->thumbSize;
            widget->vertices[6].y = data->pos + data->thumbSize;
            widget->vertices[7].y = data->pos;
        }

        widget->modified = true;
    }

    bool OnClick(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;
        data->mouseDown = true;
        widget->update = true;
        functions->stealMouse(widget);

        OnUpdate(widget, state, x, y);
        return true;
    }

    bool OnReleaseInside(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        Data* data = (Data*)widget->data;
        data->mouseDown = false;
        widget->update = false;
        functions->freeMouse(widget);

        if(!data->mouseInside)
            widget->update = false;

        return true;
    }

    bool OnReleaseOutside(GUI::Widget* widget, lua_State* state, int32_t x, int32_t y)
    {
        return OnReleaseInside(widget, state, x, y);
    }

    int QueryNumber(GUI::Widget* widget, const char* key, float* value)
    {
        if(streq(key, "value")) {
            Data* data = (Data*)widget->data;
            *value = data->currentValue;
        }
        return false;
    }

    bool SetNumber(GUI::Widget* widget, const char* key, float value)
    {
        if(streq(key, "value")) {
            Data* data = (Data*)widget->data;
            data->currentValue = std::min(data->maxValue, std::max(data->minValue, value));
            if(data->direction == Scrollbar::HORIZONTAL) {
                Scrollbar::UpdateHorizontalValue(data->currentValue
                                                    , widget->bounds
                                                    , data->minValue
                                                    , data->maxValue
                                                    , data->thumbSize
                                                    , &data->pos);

                widget->vertices[4].x = data->pos;
                widget->vertices[5].x = data->pos;
                widget->vertices[6].x = data->pos + data->thumbSize;
                widget->vertices[7].x = data->pos + data->thumbSize;
            } else {
                Scrollbar::UpdateVerticalValue(data->currentValue
                                                , widget->bounds
                                                , data->minValue
                                                , data->maxValue
                                                , data->thumbSize
                                                , &data->pos);

                widget->vertices[4].y = data->pos;
                widget->vertices[5].y = data->pos + data->thumbSize;
                widget->vertices[6].y = data->pos + data->thumbSize;
                widget->vertices[7].y = data->pos;
            }
            widget->modified = true;
            return data->currentValue == value;
        }

        return false;
    }
}
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
#include <cctype>
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

    bool percent;

    Scrollbar::DIRECTION direction;

    bool mouseDown;
    bool mouseInside;

    char* textFormat;
};
const static int TEXT_BUFFER_LENGTH = 256; // This is only the length of a temporary buffer, 256 should be fine

void UpdateText(Widget* widget) {
    Data* data = (Data*)widget->data;
    // Reset these before updating so createText can be called
    widget->offsetData.vertex = 8;
    widget->offsetData.index = 12;
    widget->offsetData.vertexBegin = 8;

    char buffer[TEXT_BUFFER_LENGTH];
    if(data->percent) {
        float percent = (data->currentValue - data->minValue) / (data->maxValue - data->minValue) * 100.0f;
        sprintf(buffer, data->textFormat, percent);
    } else {
        sprintf(buffer, data->textFormat, data->currentValue);
    }
    auto vertexOffset = widget->offsetData.vertex;
    functions->createText(widget, buffer, 255, 255, 255, 255);
    AdjustText(widget->vertices + vertexOffset, strlen(buffer), (Origin)(Origin::CENTER_VERTICAL | Origin::CENTER_HORIZONTAL), widget->bounds.x + widget->bounds.width * 0.5f, widget->bounds.y + widget->bounds.height * 0.5f);
    widget->vertexCount = 8 + strlen(buffer) * 4;
    widget->indexCount = 12 + strlen(buffer) * 6;
}

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
        data.percent = false;

        BackgroundColor::Parse(state, &data.backgroundColor, defaults);
        Scrollbar::Parse(state
                            , &data.currentValue
                            , &data.minValue
                            , &data.maxValue
                            , &data.valueIncrement
                            , &data.direction
                            , &data.thumbColor
                            , defaults);

        int maxStringLength = 0;
        if(FieldExists(state, "text_format")) {
            const char* textFormat = nullptr;
            char buffer[TEXT_BUFFER_LENGTH];
            textFormat = lua_tostring(state, -1);
            int length = strlen(textFormat);
            bool foundFormat = false;
            bool valid = true;
            // Make sure there is one and only one %f\%p
            for(int i = 0; i < length + 1 && valid; ++i) {
                if(textFormat[i] == '%') {
                    if(i + 1 < length) {
                        buffer[i] = textFormat[i];
                        if(textFormat[i + 1] == 'f') {
                            if(foundFormat) {
                                valid = false; // Multiple %
                            }
                            foundFormat = true;
                        } else if(textFormat[i + 1] == '%') {
                            buffer[i + 1] = textFormat[i + 1];
                            ++i;
                        } else {
                            if(foundFormat) {
                                valid = false;
                            }
                            foundFormat = true;
                            for(; i < length; ++i) {
                                valid = false;
                                if(std::isalpha(textFormat[i])) {
                                    if(textFormat[i] == 'p') {
                                        // Replace %p with %f, since value is still a float
                                        data.percent = true;
                                        buffer[i] = 'f';
                                        valid = true;
                                        break;
                                    } else if(textFormat[i] == 'f') {
                                        buffer[i] = 'f';
                                        valid = true;
                                        break;
                                    } else {
                                        // Only p or f is valid
                                        break;
                                    }
                                } else {
                                    buffer[i] = textFormat[i];
                                }
                            }
                        }
                    } else {
                        // Invalid, trailing %
                        valid = false;
                        break;
                    }
                } else {
                    buffer[i] = textFormat[i];
                }
            }

            if(!valid || !foundFormat) {
                strcpy(buffer, "INVALID");
            }

            data.textFormat = (char*)functions->memalloc(length + 1);
            strcpy(data.textFormat, buffer);

            if(data.percent) {
                maxStringLength = snprintf(nullptr, 0, data.textFormat, 100.0f);
            } else {
                maxStringLength = std::max(snprintf(nullptr, 0, data.textFormat, data.minValue), snprintf(nullptr, 0, data.textFormat, data.maxValue));
            }

            lua_pop(state, 1);
        } else {
            data.textFormat = nullptr;
        }

        QUAD_ALLOC(widget, 2 + std::min(maxStringLength, TEXT_BUFFER_LENGTH - 1));
        
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

        if(data->textFormat)
            UpdateText(widget);
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

        if(data->textFormat)
            UpdateText(widget);

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

    void Destroy(void* dataPtr, lua_State* state)
    {
        Data* data = (Data*)dataPtr;
        if(data->textFormat)
            functions->memdealloc(data->textFormat);
    }
}
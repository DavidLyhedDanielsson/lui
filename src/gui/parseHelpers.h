#ifndef PARSE_HELPERS_H__
#define PARSE_HELPERS_H__

#include <lua5.1/lua.hpp>
#include "guielement.h"

namespace GUI
{
    const static Color COLOR_BACKGROUND = { 103, 137, 171, 255 };
    const static Color COLOR_TEXT = { 255, 255, 255, 255 };

    const static Color COLOR_PRIMARY = { 137, 171, 205, 255 };
    const static Color COLOR_SECONDARY = { 69, 103, 137, 255 };

    namespace BackgroundColor
    {
        void Parse(lua_State* state, Color* color, int defaults = -1)
        {
            *color = GetOptionalColor(state, "bg_color", COLOR_BACKGROUND, defaults);
        }

        void Build(GUI::Widget* widget, const Color& color)
        {
            CreateColoredQuad(widget
                                , widget->bounds.x
                                , widget->bounds.y
                                , widget->bounds.width
                                , widget->bounds.height
                                , color.r
                                , color.g
                                , color.b
                                , color.a);
        }
    }

    namespace ClickableBackgroundColor
    {
        void Parse(lua_State* state, Color* color, Color* hoverColor, Color* downColor, int defaults = -1)
        {
            *color = GetOptionalColor(state, "bg_color", COLOR_BACKGROUND, defaults);
            *hoverColor = GetOptionalColor(state, "bg_hover", COLOR_PRIMARY, defaults);
            *downColor = GetOptionalColor(state, "bg_down", COLOR_SECONDARY, defaults);
        }

        void Build(GUI::Widget* widget, const Color& color)
        {
            CreateColoredQuad(widget
                                , widget->bounds.x
                                , widget->bounds.y
                                , widget->bounds.width
                                , widget->bounds.height
                                , color.r
                                , color.g
                                , color.b
                                , color.a);
        }
    }

    namespace Text
    {
        void Parse(lua_State* state, const GUI::InitFunctions* functions, char** text, GUI::Color* color, Origin* origin, int defaults = -1)
        {
            *origin = GetOrigin(state);
            *color = GetOptionalColor(state, "color", COLOR_TEXT, defaults);

            if(FieldExists(state, "text")) {
                const char* luaText;
                luaText = lua_tostring(state, -1);
                *text = (char*)functions->memalloc(strlen(luaText) + 1);
                strcpy(*text, luaText);

                lua_pop(state, 1);
            } else {
                *text = (char*)functions->memalloc(1);
                *text[0] = '\0';
            }
        }

        void Build(const InitFunctions* functions, GUI::Widget* widget, const char* text, const Color& color, Origin origin)
        {
            auto textVertexOffset = widget->offsetData.vertex;
            functions->createText(widget, text, color.r, color.g, color.b, color.a);

            float targetX = 0.0f;
            float targetY = 0.0f;
            GetPointInRect(widget->bounds, origin, &targetX, &targetY);
            AdjustText(widget->vertices + textVertexOffset, strlen(text), origin, targetX, targetY);
        }

        void Build(const InitFunctions* functions, GUI::Widget* widget, const Rect& bounds, const char* text, const Color& color, Origin origin)
        {
            auto textVertexOffset = widget->offsetData.vertex;
            functions->createText(widget, text, color.r, color.g, color.b, color.a);

            float targetX = 0.0f;
            float targetY = 0.0f;
            GetPointInRect(bounds, origin, &targetX, &targetY);
            AdjustText(widget->vertices + textVertexOffset, strlen(text), origin, targetX, targetY);
        }

        void Dealloc(const InitFunctions* functions, char* text)
        {
            functions->memdealloc(text);
        }
    }

    namespace Scrollbar
    {
        enum DIRECTION {
            VERTICAL = 0
            , HORIZONTAL
        };

        const static auto MIN_THUMB_SIZE = 12.0f;

        void Parse(lua_State* state
                    , float* minValue
                    , float* maxValue
                    , float* currentValue
                    , float* valueIncrement
                    , DIRECTION* direction
                    , Color* thumbColor
                    , int defaults = -1)
        {
            const char* possibleValues[] = {
                "vertical"
                , "horizontal"
            };

            *currentValue = GetOptionalNumber(state, "value", 0.0f, defaults);
            *minValue = GetOptionalNumber(state, "value_min", 0.0f, defaults);
            *maxValue = GetOptionalNumber(state, "value_max", 1.0f, defaults);
            // value moves in increments of this value, also determinates thumb size
            *valueIncrement = GetOptionalNumber(state, "value_increment", 0.01f, defaults);
            *direction = (DIRECTION)GetOptionalListIndex(state, "direction", possibleValues, 2, 1, defaults);
            *thumbColor = GetOptionalColor(state, "thumb_color", COLOR_PRIMARY, defaults);
        }

        void BuildHorizontal(GUI::Widget* widget
                                , const Rect& bounds
                                , const float currentValue
                                , const float minValue
                                , const float maxValue
                                , const float valueIncrement
                                , float* thumbSize
                                , float* posX
                                , Color color)
        {
            const float totalIncrements = (maxValue - minValue) / valueIncrement;
            *thumbSize = std::max(MIN_THUMB_SIZE, bounds.width / totalIncrements);
            *posX = bounds.x + (currentValue / (maxValue - minValue) * (bounds.width - *thumbSize));

            CreateColoredQuad(widget
                                , *posX
                                , bounds.y
                                , *thumbSize
                                , bounds.height
                                , color.r
                                , color.g
                                , color.b
                                , color.a);
        }

        void BuildVertical(GUI::Widget* widget
                            , const Rect& bounds
                            , const float currentValue
                            , const float minValue
                            , const float maxValue
                            , const float valueIncrement
                            , float* thumbSize
                            , float* posY
                            , Color color)
        {
            const float totalIncrements = (maxValue - minValue) / valueIncrement;
            *thumbSize = std::max(MIN_THUMB_SIZE, bounds.height / totalIncrements);
            *posY = bounds.y + (currentValue / (maxValue - minValue) * (bounds.height - *thumbSize));

            CreateColoredQuad(widget
                                , bounds.x
                                , *posY
                                , bounds.width
                                , *thumbSize
                                , color.r
                                , color.g
                                , color.b
                                , color.a);
        }

        void UpdateHorizontal(int32_t clickPos
                                , const Rect& bounds
                                , const float thumbSize
                                , const float minValue
                                , const float maxValue
                                , float* value
                                , float* position)
        {
            clickPos = clickPos - bounds.x - thumbSize * 0.5f;
            float size = bounds.width - thumbSize;
            float percent = clickPos / size;
            percent = std::max(percent, 0.0f);
            percent = std::min(percent, 1.0f);

            *value = percent * (maxValue - minValue);
            *position = bounds.x + percent * size;
        }

        void UpdateVertical(int32_t clickPos
                                , const Rect& bounds
                                , const float thumbSize
                                , const float minValue
                                , const float maxValue
                                , float* value
                                , float* position)
        {
            clickPos = clickPos - bounds.y - thumbSize * 0.5f;
            float size = bounds.height - thumbSize;
            float percent = clickPos / size;
            percent = std::max(percent, 0.0f);
            percent = std::min(percent, 1.0f);

            *value = percent * (maxValue - minValue);
            *position = bounds.y + percent * size;
        }

        void UpdateHorizontalValue(float value
                                    , const Rect& bounds
                                    , const float thumbSize
                                    , const float minValue
                                    , const float maxValue
                                    , float* position)
        {
            float percent = value / (maxValue - minValue);
            float size = bounds.width - thumbSize;
            percent = std::max(percent, 0.0f);
            percent = std::min(percent, 1.0f);

            *position = bounds.x + percent * size;
        }

        void UpdateVerticalValue(float value
                                    , const Rect& bounds
                                    , const float thumbSize
                                    , const float minValue
                                    , const float maxValue
                                    , float* position)
        {
            float percent = value / (maxValue - minValue);
            float size = bounds.height - thumbSize;
            percent = std::max(percent, 0.0f);
            percent = std::min(percent, 1.0f);

            *position = bounds.y + percent * size;
        }
    }

    namespace Padding
    {
        void Parse(lua_State* state
                    , float* top
                    , float* bottom
                    , float* left
                    , float* right
                    , int defaults = -1)
        {
            float padding = GetOptionalNumber(state, "padding", 0.0f);

            *top = GetOptionalNumber(state, "padding_top", padding, defaults);
            *bottom = GetOptionalNumber(state, "padding_bottom", padding, defaults);
            *left = GetOptionalNumber(state, "padding_left", padding, defaults);
            *right = GetOptionalNumber(state, "padding_right", padding, defaults);
        }
    }
}

#endif
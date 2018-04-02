--[[
    To avoid having to type out "type = ...", this "inferred" table can be used
    to associate attributes with element types.

    It is parsed in top-to-bottom order, meaning that even though both button
    and text widgets have a "text" attribute, any element with both a "text"
    and "on_click" attribute will be considered to be a button, since "on_click"
    comes first.
]]
inferred = {
    { scroll = "max_height" }
    , { button = "on_click" }
    , { color = "color" } 
    , { text = "text" }
    , { linear = "direction" }
}

-- A very simple function (basically a macro) to reduce typing
function text(text)
    return {
        text = text
    }
end

-- A more complicated function
function checkbox(text, checked)
    return {
        type = "checkbox"
        , bg_color = "#00000000"
        , width = -1
        , text = text
        , checked = checked
    }
end

buttonText = {
    "First"
    , "Second"
    , "Third"
    , "Fourth"
    , "Fifth"
    , "Sixth"
    , "Seventh"
    , "Eighth"
    , "Ninth"
    , "Tenth"
}
function TestButton(i)
    local bt = ""
    if i <= #buttonText then
        bt = buttonText[i]
    else
        bt = string.format("%d", i)
    end

    return {
        text = bt
        , on_click = bt
        , width = -1
    }
end

function ColoredArea(x, y, width, height, color)
    return {
        area = { x, y, width, height }
        , element = { color = color }
    }
end

layout = {
    type = "floating"
    , defaults = {
        bg_color = "#00000000"
    }
    , ColoredArea(64, 64, 450, 450, "#456789")
    , {
        area = { 64, 64, 450, 450 }
        , element = {
            direction = "v"
            , padding_left = -1
            , padding_right = -1
            , {
                text = "Some type of menu"
                , width = -1
                , align_horizontal = "center"
            }
            , {
                direction = "h"
                , width = -1
                , padding_left = -1
                , padding_right = -1
                , padding_element = -1
                , {
                    direction = "v"
                    , checkbox("Setting 1", true)
                    , checkbox("Left", false)
                    , checkbox("Setting 3", false)
                }
                , {
                    direction = "v"
                    , checkbox("Setting 1", true)
                    , checkbox("Right", true)
                    , checkbox("Setting 3", false)
                }
            }
            , {
                direction = "h"
                , width = -1
                , padding_left = -1
                , padding_right = -1
                , padding_element = -1
                , text("Some slider")
                , {
                    type = "slider"
                    , bg_color = "#234567FF"
                    , width = 256
                    , height = 36
                }
            }
            , {
                direction = "h"
                , width = -1
                , padding_left = -1
                , padding_right = -1
                , padding_element = -1
                , {
                    direction = "v"
                    , text("Settings dropdown")
                    , text("Scrolling dropdown")
                }
                , {
                    direction = "v"
                    , defaults = {
                        bg_color = "#456789FF"
                    }
                    , width = 200
                    , {
                        type = "dropdown"
                        , width = -1
                        , text = "I change text"
                        , style = "options"
                        , dropdown_height = 256
                        , TestButton(1)
                        , TestButton(2)
                        , TestButton(3)
                    }
                    , {
                        type = "dropdown"
                        , width = -1
                        , text = "Click"
                        , style = "dropdown"
                        , dropdown_height = 128
                        , TestButton(1)
                        , TestButton(2)
                        , TestButton(3)
                        , TestButton(4)
                        , TestButton(5)
                        , TestButton(6)
                        , TestButton(7)
                        , TestButton(8)
                        , TestButton(9)
                        , TestButton(10)
                    }
                    , {
                        type = "dropdown"
                        , width = -1
                        , text = "Context menu"
                        , style = "context"
                        , {
                            type = "dropdown"
                            , width = -1
                            , text = "Nested"
                            , style = "context"
                            , TestButton(1)
                            , TestButton(2)
                            , TestButton(3)
                        }
                        , TestButton(1)
                        , TestButton(2)
                        , TestButton(3)
                        , TestButton(4)
                        , TestButton(5)
                        , TestButton(6)
                    }
                }
            }
            , {
                width = -1
                , max_height = 150
                , defaults = {
                    bg_color = "#234567FF" -- To set scrollbar background
                }
                , {
                    direction = "v"
                    , defaults = {
                        bg_color = "#00000000" -- To set button background
                    }
                    , text("Elements inside scrolling area")
                    , {
                        direction = "h"
                        , text("Slider1")
                        , {
                            type = "slider"
                            , bg_color = "#234567FF" -- To set scrollbar background
                        }
                    }
                    , {
                        direction = "h"
                        , text("Slider2")
                        , {
                            type = "slider"
                            , bg_color = "#234567FF" -- To set scrollbar background
                        }
                    }
                    , TestButton(1)
                    , TestButton(2)
                    , TestButton(3)
                    , TestButton(4)
                    , TestButton(5)
                    , TestButton(6)
                    , TestButton(7)
                    , TestButton(8)
                    , TestButton(9)
                }
            }
        }
    }
}

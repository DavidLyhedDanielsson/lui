#include "gui.h"
#include "luahelpers.h"
#include "lib.h"

#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <map>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <vector>
#include <stack>
#include <map>
#include <algorithm>
#include <iostream>
#include <dlfcn.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H

#ifdef VALGRIND
#include <valgrind/valgrind.h>
#endif

#include <timer.h>

// TODO: Keep everything the same after reloading
// TODO: Round vertices to nearest pixel whereever

namespace GUI
{
    const static size_t TYPE_MAX_LENGTH = 64;
    const static size_t PATH_MAX_LENGTH = 256;
    const static size_t NAME_MAX_LENGTH = 64;

    // Setup
    typedef void    (*InitFunction)(int, const InitFunctions*);
    typedef int     (*CountFunction)(lua_State*);
    typedef void    (*MeasureFunction)(lua_State*, int32_t* width, int32_t* height);
    typedef int     (*ParseLayoutFunction)(lua_State*, Layout*, Widget*, int);
    typedef int     (*ParseWidgetFunction)(lua_State*, Widget*, int);
    typedef void    (*BuildLayoutFunction)(Layout*, Element**, int32_t);
    typedef int     (*BuildWidgetFunction)(Widget* widget);
    typedef void    (*BuildChildrenFunction)(Widget*, Element**, int32_t);
    typedef void    (*DestroyFunction)(void*, lua_State*);
    // Events
    typedef void (*OnEnterFunction)(Widget*, lua_State*);
    typedef void (*OnExitFunction)(Widget*, lua_State*);
    typedef bool (*OnClickFunction)(Widget*, lua_State*, int32_t, int32_t);
    typedef bool (*OnReleaseInsideFunction)(Widget*, lua_State*, int32_t, int32_t);
    typedef bool (*OnReleaseOutsideFunction)(Widget*, lua_State*, int32_t, int32_t);
    typedef bool (*OnScrollFunction)(Element*, lua_State*, int32_t, int32_t);
    typedef void (*OnUpdateFunction)(Widget*, lua_State*, int32_t, int32_t);
    typedef void (*OnPopupClosedFunction)(Widget*, lua_State*, bool);
    typedef bool (*OnChildClickedFunction)(Element*, lua_State*, Widget*, int32_t, int32_t);
    typedef bool (*OnChildReleasedFunction)(Element*, lua_State*, Widget*, int32_t, int32_t);
    typedef bool (*OnChildUpdateFunction)(Element*, lua_State*, Widget*, int32_t, int32_t);
    // Queries
    typedef bool (*QueryNumberFunction)(Element* widget, const char* key, float* value);
    typedef int (*QueryStringFunction)(Element* widget, const char* key, char* value, int32_t);
    // Setters
    typedef bool (*SetNumberFunction)(Element* widget, const char* key, float value);
    typedef bool (*SetStringFunction)(Element* widget, const char* key, const char* value);

    struct Extension
    {
        char path[PATH_MAX_LENGTH];
        char name[NAME_MAX_LENGTH];
        void* libraryHandle;
        // Setup
        InitFunction initFunction;
        CountFunction countFunction;
        MeasureFunction measureFunction;
        DestroyFunction destroyFunction;
        ParseLayoutFunction parseLayoutFunction;
        ParseWidgetFunction parseWidgetFunction;
        BuildLayoutFunction buildLayoutFunction;
        BuildWidgetFunction buildWidgetFunction;
        BuildChildrenFunction buildChildrenFunction;
        // Events
        OnEnterFunction onEnterFunction;
        OnExitFunction onExitFunction;
        OnClickFunction onClickFunction;
        OnReleaseInsideFunction onReleaseInsideFunction;
        OnReleaseOutsideFunction onReleaseOutsideFunction;
        OnScrollFunction onScrollFunction;
        OnUpdateFunction onUpdateFunction;
        OnPopupClosedFunction onPopupClosedFunction;
        OnChildClickedFunction onChildClickedFunction;
        OnChildReleasedFunction onChildReleasedFunction;
        OnChildUpdateFunction onChildUpdateFunction;
        // Queries
        QueryNumberFunction queryNumberFunction;
        QueryStringFunction queryStringFunction;
        // Setters
        SetNumberFunction setNumberFunction;
        SetStringFunction setStringFunction;
    };
    static std::vector<Extension> extensions;

    struct TypeInferInfo 
    {
        char type[TYPE_MAX_LENGTH];
        std::vector<std::string> members;
    };
    static std::vector<TypeInferInfo> typeInferInfo;

    struct Popup
    {
        int32_t parent; // The widgets that opened the popup
        CLOSE_ON closeOn; // Controls what closes the popup (clicks, mouse exiting it)
        int32_t hoveredWidget;
        int32_t downWidget;
        // Every popup gets its own widget mask.
        // All widgets in a popup has the same widget mask as the popup itself
        int32_t widgetMask;
    };
    static std::vector<Popup> popups;

    // Auto reload lua file and extensions?
    static bool autoReload = true;
    static char sourcePath[PATH_MAX_LENGTH];
    static int inotifyHandle = -1;

    static size_t resolutionX;
    static size_t resolutionY;

    // All widgets in the entire GUI
    static std::vector<Widget> widgets;
    static Layout* rootLayout;
    static std::vector<Layout*> preparsedLayouts;

    static std::vector<DrawList> drawLists;
    // All vertices and indicies, updates as needed
    static std::vector<Vertex> vertices;
    static std::vector<uint32_t> indicies;

    static std::stack<int32_t> defaultsStack;

    static std::map<std::string, int32_t> namedWidgets;
    static std::map<std::string, Layout*> namedLayouts;

    // These are indicies into the widgets list.
    // A popup has its own hoveredWidget and downWidget; these are only used
    // for widgets outside of popups
    static int32_t hoveredWidget = -1;
    static int32_t downWidget = -1;

    Rect UnpackClipRect(uint64_t clipRect) {
        return { (float)(clipRect & 0xFFFF)
                    , (float)(clipRect >> 16 & 0xFFFF)
                    , (float)(clipRect >> 32 & 0xFFFF)
                    , (float)(clipRect >> 48 & 0xFFFF)
        };
    }

    // Returns the extension ID based on the type infer info in the lua file
    int InferType(lua_State* state)
    {
        const char* type = nullptr;
        for(const TypeInferInfo& info : typeInferInfo) {
            type = info.type;
            for(const std::string& members : info.members) {
                lua_getfield(state, -1, members.c_str());
                if(lua_isnil(state, -1)) {
                    type = nullptr;
                    lua_pop(state, 1);
                    break;
                }
                lua_pop(state, 1);
            }
            if(type != nullptr)
                break;
        }

        if(type == nullptr)
            return -1;

        int index = -1;
        for(size_t i = 0; i < extensions.size(); ++i) {
            if(streq(extensions[i].name, type)) {
                index = (int)i;
                break;
            }
        }

        if(index == -1)
            return -1;
        
        return index;
    }

    // Returns the extension ID of an widget
    // Prints an error message and an widget dump if it couldn't be found
    int GetExtension(lua_State* state)
    {
        int extensionIndex = -1;

        char type[TYPE_MAX_LENGTH];
        if(!GetString(state, "type", type, TYPE_MAX_LENGTH, nullptr)) {
            extensionIndex = InferType(state);

            if(extensionIndex == -1) {
                std::cerr << "No type given and couldn't infer type" << std::endl;
                DumpElement(state);
            }
        } else {
            for(size_t i = 0; i < extensions.size(); ++i) {
                if(streq(extensions[i].name, type)) {
                    extensionIndex = i;
                    break;
                }
            }

            if(extensionIndex == -1) {
                std::cerr << "Unknown layout type \"" << type << "\"" << std::endl;
            }
        }

        return extensionIndex;
    }

    // Callback from GUI messages
    // TODO: Make this configurable
    void ReceiveMessage(const char* message)
    {
        std::cout << "LUA MESSAGE: " << message << std::endl;
    }

    // Default memory allocation callback
    void* memalloc(uint64_t size)
    {
        return malloc(size);
    }

    // Default memory deallocation callback
    void memdealloc(void* ptr)
    {
        free(ptr);
    }

    void OpenPopup(Element** elements, int32_t elementCount, CLOSE_ON closeOn);
    void ClosePopup(lua_State* state);
    int CountElements(lua_State* state);
    void MeasureElements(lua_State* state, int32_t* width, int32_t* height);
    int ParseLayout(lua_State* state, Widget* elements);

    void SetDrawRec(Element* element, bool draw, int32_t maxDepth, int32_t currentDepth)
    {
        if(element->type == WIDGET) {
            ((Widget*)element)->draw = draw;
            ++currentDepth;
        }
        if(maxDepth == -1 || currentDepth < maxDepth) { 
            for(size_t i = 0; i < element->children.size(); ++i) {
                SetDrawRec(element->children[i], draw, maxDepth, currentDepth);
            }
        }
    }
    // Sets whether or not the given widgets should be visible
    // Depth is increased when a child is a widget
    void SetDraw(Element* element, bool draw, int32_t depth)
    {
        SetDrawRec(element, draw, depth, 0);
    }

    void SetLayerRec(Element* element, int32_t draw, int32_t maxDepth, int32_t currentDepth)
    {
        if(element->type == WIDGET) {
            ((Widget*)element)->layer = draw;
            ++currentDepth;
        }
        if(maxDepth == -1 || currentDepth < maxDepth) { 
            for(size_t i = 0; i < element->children.size(); ++i) {
                SetLayerRec(element->children[i], draw, maxDepth, currentDepth);
            }
        }
    }
    // Sets the layer of the given widgets
    // Depth is increased when a child is a widget
    void SetLayer(Element* element, int32_t layer, int32_t depth)
    {
        SetLayerRec(element, layer, depth, 0);
    }

    void SetMask(Element* element, int32_t mask)
    {
        if(element->type == WIDGET) {
            ((Widget*)element)->mask = mask;
        }
        for(size_t i = 0; i < element->children.size(); ++i) {
            SetMask(element->children[i], mask);
        }
    }

    void SetClipRect(Element* element, uint64_t clipRect)
    {
        if(element->type == WIDGET) {
            ((Widget*)element)->clipRect = clipRect;
        }
        for(size_t i = 0; i < element->children.size(); ++i) {
            SetClipRect(element->children[i], clipRect);
        }
    }

    void SetClipRect(Element* element, Rect clipRect)
    {
        uint64_t packedClipRect = ((uint64_t)clipRect.x & 0xFFFF)
                                    | ((uint64_t)clipRect.y & 0xFFFF) << 16
                                    | ((uint64_t)clipRect.width & 0xFFFF) << 32
                                    | ((uint64_t)clipRect.height & 0xFFFF) << 48;

        SetClipRect(element, packedClipRect);
    }

    bool QueryNumber(Element* element, const char* key, float* value)
    {
        if(extensions[element->extension].queryNumberFunction)
            return extensions[element->extension].queryNumberFunction(element, key, value);
        
        return false;
    }

    int QueryString(Element* element, const char* key, char* value, int32_t maxLength)
    {
        if(extensions[element->extension].queryStringFunction)
            return extensions[element->extension].queryStringFunction(element, key, value, maxLength);
        
        return -1;
    }

    bool SetNumber(Element* element, const char* key, float value)
    {
        if(extensions[element->extension].setNumberFunction)
            return extensions[element->extension].setNumberFunction(element, key, value);
        
        return false;
    }

    bool SetString(Element* element, const char* key, const char* value)
    {
        if(extensions[element->extension].setStringFunction)
            return extensions[element->extension].setStringFunction(element, key, value);
        
        return false;
    }

    static std::unordered_map<uint32_t, Character> characters;

    const static Character& GetCharacter(uint32_t characterCode)
    {
        auto iter = characters.find(characterCode);
        if(iter == characters.end()) {
            iter = characters.find(63); // 63 = '?' ASCII
        }

        return iter->second;
    }

    int fontAscend;
    int fontDescend;
    int fontHeight;
    void MeasureText(const char* text
                        , int32_t* width
                        , int32_t* height)
    {
        const auto textLength = strlen(text);

        int x = 0;
        int y = 0;

        for(size_t i = 0; i < textLength; ++i) {
            Character character = GetCharacter(text[i]);

            x += character.xAdvance;
            y = std::max(y, (int)character.height);
        }

        if(width)
            *width = x;
        if(height)
            *height = y;
    }

    int GetFontHeight()
    {
        return fontHeight + fontAscend;
    }

    void CreateText(Widget* widget
                        , const char* text
                        , uint8_t r
                        , uint8_t g
                        , uint8_t b
                        , uint8_t a)
    {
        const auto textLength = strlen(text);

        float x = 0.0f;
        float y = 0.0f;

        for(size_t i = 0; i < textLength; ++i) {
            Character character = GetCharacter(text[i]);

            CreateQuad(widget->vertices + widget->offsetData.vertex + i * 4
                        , widget->indicies + widget->offsetData.index + i * 6
                        , widget->offsetData.vertexBegin + i * 4
                        , x + character.xOffset
                        , y + fontHeight - character.yOffset
                        , character.width
                        , character.height
                        , character.uMin
                        , character.uMax
                        , character.vMin
                        , character.vMax
                        , r
                        , g
                        , b
                        , a);

            x += character.xAdvance;
        }

        widget->offsetData.vertex += textLength * 4;
        widget->offsetData.index += textLength * 6;
        widget->offsetData.vertexBegin += textLength * 4;
    }

    void CreateFont(const char* path, uint8_t* imageData, int imageWidth, int imageHeight)
    {
        FT_Library ftLibrary;
        FT_Init_FreeType(&ftLibrary);

        FT_Face face;
        FT_New_Face(ftLibrary, path, 0, &face);
        FT_Set_Pixel_Sizes(face, 0, 24);
        fontAscend = face->ascender >> 6;
        fontDescend = face->descender >> 6;
        fontHeight = face->size->metrics.height >> 6;

        int x = 2;
        int y = 0;

        std::vector<uint32_t> characterCodes;
        characterCodes.push_back(8230);
        for(uint32_t i = 32; i <= 126; ++i) {
            characterCodes.push_back(i);
        }

        for(int i = 0; i < 8; ++i)
            imageData[i] = 255;
        for(int i = 0; i < 8; ++i)
            imageData[imageWidth * 4 + i] = 255;

        FT_GlyphSlot slot = face->glyph;
        for(uint32_t i = 0; i < characterCodes.size(); ++i) {
            FT_Load_Char(face, characterCodes[i], FT_LOAD_RENDER);
            
            uint16_t width = (uint16_t)slot->metrics.width >> 6;
            uint16_t height = (uint16_t)slot->metrics.height >> 6;

            Character character;
            if(x + width >= imageWidth) {
                x = 0;
                y += 24 + 1;
            }
            character.uMin = x / (float)imageWidth;
            character.uMax = (x + width) / (float)imageWidth;
            character.vMin = y / (float)imageHeight;
            character.vMax = (y + height) / (float)imageHeight;
            character.width = width;
            character.height = height;
            character.xOffset = (uint16_t)slot->metrics.horiBearingX >> 6;
            character.yOffset = (uint16_t)slot->metrics.horiBearingY >> 6;
            character.xAdvance = (uint16_t)slot->advance.x >> 6;
            characters.insert(std::make_pair(characterCodes[i], character));

            for(int yy = 0; yy < character.height; ++yy) {
                for(int xx = 0; xx < character.width; ++xx) {
                    int imageIndex = (y + yy) * imageWidth * 4 + (x + xx) * 4;

                    imageData[imageIndex] = 255;
                    imageData[imageIndex + 1] = 255;
                    imageData[imageIndex + 2] = 255;
                    imageData[imageIndex + 3] = slot->bitmap.buffer[(height - 1 - yy) * slot->bitmap.width + xx];
                }
            }

            x += width + 1;
        }

        FT_Done_FreeType(ftLibrary);
    }

    static Element* mouseOwnElement = nullptr;
    void StealMouse(Element* element)
    {
        mouseOwnElement = element;
    }

    void FreeMouse(Element* element)
    {
        if(mouseOwnElement == element)
            mouseOwnElement = nullptr;
    }

    Element* GetNamedElement(const char* name)
    {
        {
            auto iter = namedWidgets.find(name);
            if(iter != namedWidgets.end())
                return &widgets[iter->second];
        }
        {
            auto iter = namedLayouts.find(name);
            if(iter != namedLayouts.end())
                return iter->second;
        }

        return nullptr;
    }

    void BuildLayouts(Element*);

    // This struct is sent to all widgets when init is called
    const InitFunctions initFunctions {
        ReceiveMessage
        , CountElements
        , MeasureElements
        , ParseLayout
        , BuildLayouts
        , OpenPopup
        , ClosePopup
        , memalloc
        , memdealloc
        , SetDraw
        , QueryNumber
        , QueryString
        , SetNumber
        , SetString
        , SetClipRect
        , CreateText
        , MeasureText
        , StealMouse
        , FreeMouse
        , GetNamedElement
    };

    // These are needed to keep track of if a popup is opened while the mouse is held,
    // in which case some special behaviour is needed
    bool popupOpened = false;
    bool mouseDown = false;

    // Currently, when opening a popup, this value is incremented once and that
    // is the popup's widget mask
    int32_t popupWidgetMask = 0;
    void OpenPopup(Element** popupElements, int32_t elementCount, CLOSE_ON closeOn)
    {
        int32_t parent;
        if(!popups.empty()) {
                parent = popups.back().hoveredWidget;
        } else {
            if(hoveredWidget != -1)
                parent = hoveredWidget;
        }

        Popup popup = { parent, closeOn, -1, -1, popupWidgetMask };

        int32_t layer = 1;
        for(int32_t i = 0; i < (int32_t)widgets.size(); ++i) {
            layer = std::max(layer, widgets[i].layer + 1);
        }
        for(int32_t i = 0; i < elementCount; ++i) {
            SetDraw(popupElements[i], true, 1);
            SetLayer(popupElements[i], layer, 1);
            SetMask(popupElements[i], popupWidgetMask);
        }

        popupWidgetMask++;

        popups.push_back(popup);

        popupOpened = mouseDown;
    }

    void DestroyLayout(Layout* layout, lua_State* state)
    {
        if(layout->data && extensions[layout->extension].destroyFunction) {
            extensions[layout->extension].destroyFunction(layout->data, state);
        }

        free(layout->data);
        delete layout;
    }

    void DestroyWidget(Widget* widget, lua_State* state)
    {
        if(widget->data && extensions[widget->extension].destroyFunction) {
            extensions[widget->extension].destroyFunction(widget->data, state);
        }

        free(widget->data);
        free(widget->vertices);
        free(widget->indicies);
    }
    
    // Recursively destroys layouts
    void DestroyLayouts(Element* element, lua_State* state)
    {
        for(size_t i = 0; i < element->children.size(); ++i) {
            DestroyLayouts(element->children[i], state);
        }

        if(element->type == LAYOUT)
            DestroyLayout((Layout*)element, state);
    }

    // Completely destroys the gui and deallocates any dynamic memory
    // keepExtensions can be used to unload and then reload all shared libraries. Used when reloading
    void DestroyGUI(lua_State* state, bool keepExtensions/*= false*/)
    {
        std::vector<std::pair<std::string, std::string> > extensionPaths;
        if(keepExtensions) {
            extensionPaths.reserve(extensions.size());
            for(size_t i = 0; i < extensions.size(); ++i)
                extensionPaths.push_back(std::make_pair<std::string, std::string>(extensions[i].name, extensions[i].path));
        }

        DestroyLayouts(rootLayout, state);
        for(size_t i = 0; i < preparsedLayouts.size(); ++i) {
            DestroyLayouts(preparsedLayouts[i], state);
        }
        preparsedLayouts.resize(0);
        for(size_t i = 0; i < widgets.size(); ++i)
            DestroyWidget(&widgets[i], state);
        
#ifdef VALGRIND
        // In all likelyhood, if extensions aren't kept, we're closing the
        // program, so just don't if we need to debug them
        if(!RUNNING_ON_VALGRIND && !keepExtensions)
#endif
        for(size_t i = 0; i < extensions.size(); ++i)
            dlclose(extensions[i].libraryHandle);

        if(keepExtensions) {
            extensions.clear();
            extensions.reserve(extensionPaths.size());
            for(size_t i = 0, size = extensionPaths.size(); i < size; ++i)
                RegisterSharedLibrary(&extensionPaths[i].first[0], &extensionPaths[i].second[0]);
        } else {
            extensions.clear();
        }

        vertices.resize(0);
        drawLists.resize(0);
        widgets.resize(0);
        typeInferInfo.resize(0);
        popups.resize(0);
        namedWidgets.clear();
        namedLayouts.clear();
        while(!defaultsStack.empty()) {
            if(defaultsStack.top() != -1) // I don't think -1 is a valid ref index?
                luaL_unref(state, LUA_REGISTRYINDEX, defaultsStack.top());
            defaultsStack.pop();
        }

        hoveredWidget = -1;
        downWidget = -1;
    }

    void ClosePopups(lua_State* state, int32_t count)
    {
        if(count > (int32_t)popups.size())
            count = popups.size();

        for(int32_t i = 0; i < count; ++i) {
            //for(int32_t j = 0; j < popups.back().widgetCount; ++j) {
            for(int32_t j = 0; j < (int32_t)widgets.size(); ++j) {
                Widget* widget = &widgets[j];
                if(widget->mask != popups.back().widgetMask)
                    continue;

                if(extensions[widget->extension].onExitFunction) {
                    extensions[widget->extension].onExitFunction(widget, state);
                }
                widget->draw = false;
                widget->mask = -1;
            }

            if(popups.back().parent != -1) {
                Widget* widget = &widgets[popups.back().parent];
                if(extensions[widget->extension].onPopupClosedFunction)
                    extensions[widget->extension].onPopupClosedFunction(widget, state, mouseDown);
            }
            popups.pop_back();
        }
    }

    // At this point an element only needs to be able to close the one popoup
    // that it opens
    void ClosePopup(lua_State* state)
    {
        ClosePopups(state, 1);
    }

    std::vector<TypeInferInfo> ParseTypeInferInfo(lua_State* state)
    {
        std::vector<TypeInferInfo> returnInfo;

        size_t count = lua_objlen(state, -1);
        for(size_t i = 0; i < count; ++i) {
            lua_rawgeti(state, -1, i + 1);

            lua_pushnil(state);
            while(lua_next(state, -2)) {
                TypeInferInfo info;
                strcpy(info.type, lua_tostring(state, -2)); // TODO: Bounds checking

                if(lua_istable(state, -1)) {
                    size_t elementCount = lua_objlen(state, -1);
                    info.members.reserve(elementCount);
                    for(size_t i = 0; i < elementCount; ++i) {
                        lua_rawgeti(state, -1, i + 1);
                        if(lua_isstring(state, -1))
                            info.members.push_back(lua_tostring(state, -1));
                        lua_pop(state, 1);
                    }
                } else if(lua_isstring(state, -1)) {
                    info.members.push_back(lua_tostring(state, -1));
                }
                returnInfo.push_back(info);
                lua_pop(state, 1);
            }

            lua_pop(state, 1);
        }

        return returnInfo;
    }

    void ShallowCopyInto(lua_State* state)
    {
        // dest, source
        lua_pushnil(state);
        // dest, source, nil
        while(lua_next(state, -2)) {
            // Duplicate key and keep one, insert key and value into new table
            // dest, source, key, value
            lua_pushvalue(state, -2);
            // dest, source, key, value, key
            lua_insert(state, -2);
            // dest, source, key, key, value
            lua_settable(state, -5);
            // dest, source, key
        }
    }

    static std::vector<Element*> layoutsStack;

    int ParseLayout(lua_State* state, Widget* widgets)
    {
        int extensionIndex = GetExtension(state);
        if(extensionIndex == -1)
            return 0;

        int defaults = -1;
        bool pushedDefaults = false;
        if(FieldExists(state, "defaults")) {
            defaultsStack.push(luaL_ref(state, LUA_REGISTRYINDEX));
            pushedDefaults = true;
        } else if(FieldExists(state, "inherited_defaults")) {
            if(!defaultsStack.empty()) {
                lua_rawgeti(state, LUA_REGISTRYINDEX, defaultsStack.top());
                ShallowCopyInto(state);
                lua_pop(state, 1);
            }
            defaultsStack.push(luaL_ref(state, LUA_REGISTRYINDEX));
            pushedDefaults = true;
        }
        
        if(!defaultsStack.empty())
            defaults = defaultsStack.top();

        std::string name;
        if(FieldExists(state, "name")) {
            name = lua_tostring(state, -1);
            lua_pop(state, 1);
        }

        int returnValue = 0;
        bool pop = false;
        if(extensions[extensionIndex].parseLayoutFunction) {
            Layout* newLayout = new Layout();
            newLayout->extension = extensionIndex;
            newLayout->data = nullptr;
            newLayout->parent = nullptr;
            if(!layoutsStack.empty()) {
                layoutsStack.back()->children.push_back(newLayout);
                pop = true;

                if(!layoutsStack.empty()) {
                    newLayout->parent = layoutsStack.back();
                }
            }
            layoutsStack.push_back(newLayout);

            returnValue = extensions[extensionIndex].parseLayoutFunction(state, newLayout, widgets, defaults);

            if(pop)
                layoutsStack.pop_back();

            if(!name.empty()) {
                auto widgetIter = namedWidgets.find(name);
                auto layoutIter = namedLayouts.find(name);
                if(widgetIter == namedWidgets.end() && layoutIter == namedLayouts.end()) {
                    namedLayouts[name] = newLayout;
                } else {
                    std::cerr << "Multiple elements named " << name << std::endl;
                }
            }
        } else {
            widgets->draw = true;
            widgets->update = false;
            widgets->modified = false;
            widgets->layer = 0;
            widgets->extension = extensionIndex;
            widgets->vertices = nullptr;
            widgets->data = nullptr;
            widgets->vertexCount = 0;
            widgets->parent = nullptr;
            widgets->mask = -1;
            widgets->clipRect = 0;
            widgets->offsetData = { 0, 0, 0 };

            if(!layoutsStack.empty()) {
                widgets->parent = layoutsStack.back();
            }

            layoutsStack.push_back(widgets);

            returnValue = extensions[extensionIndex].parseWidgetFunction(state, widgets, defaults);

            layoutsStack.pop_back();

            layoutsStack.back()->children.push_back(widgets);

            if(!name.empty()) {
                auto widgetIter = namedWidgets.find(name);
                auto layoutIter = namedLayouts.find(name);
                if(widgetIter == namedWidgets.end() && layoutIter == namedLayouts.end()) {
                    namedWidgets[name] = (int32_t)(GUI::widgets.data() - widgets);
                } else {
                    std::cerr << "Multiple elements named " << name << std::endl;
                }
            }
        }

        if(pushedDefaults) {
            luaL_unref(state, LUA_REGISTRYINDEX, defaultsStack.top());
            defaultsStack.pop();
        }

        return returnValue;
    }

    void BuildLayouts(Element* element)
    {
        if(element->type == LAYOUT) {
            extensions[element->extension].buildLayoutFunction((Layout*)element, element->children.data(), (int32_t)element->children.size());
        } else {
            Widget* guiWidget = (Widget*)element;
            guiWidget->modified = true;
            if(!element->children.empty())
                extensions[element->extension].buildChildrenFunction((Widget*)element, element->children.data(), (int32_t)element->children.size());

            ((Widget*)element)->offsetData = { 0, 0, 0 };
            extensions[element->extension].buildWidgetFunction((Widget*)element);
        }

        for(int32_t i = 0; i < (int32_t)element->children.size(); ++i) {
            BuildLayouts(element->children[i]);
        }
    }

    void MeasureElements(lua_State* state, int32_t* width, int32_t* height)
    {
        int extensionIndex = GetExtension(state);
        if(extensionIndex == -1)
            return;

        if(extensions[extensionIndex].measureFunction) {
            extensions[extensionIndex].measureFunction(state, width, height);
        } else {
            *width = -1;
            *height = -1;
        }
    }

    int CountElements(lua_State* state)
    {
        int extensionIndex = GetExtension(state);
        if(extensionIndex == -1)
            return 0;

        if(extensions[extensionIndex].countFunction)
            return extensions[extensionIndex].countFunction(state);
        else
            return 1;
    }

    void BuildGUI(lua_State* state)
    {
        Timer timer;
        Timer destroyTime;
        Timer luaTime;
        Timer countTime;
        Timer parseTime;
        Timer buildLayoutsTime;
        timer.Start();

        if(!widgets.empty()) {
            destroyTime.Start();
            DestroyGUI(state, true);
            destroyTime.Stop();
        }

        luaTime.Start();

        int status = luaL_loadfile(state, sourcePath);
        if(status != 0) {
            const char* error = lua_tostring(state, -1);
            if(error)
                std::cerr << "Lua error when building GUI: " << error << std::endl;
            else
                std::cerr << "Lua error when building GUI but no error message was available" << std::endl;
            return;
        }
        status = lua_pcall(state, 0, 0, 0);
        luaTime.Stop();
        if(status != 0) {
            const char* error = lua_tostring(state, -1);
            if(error)
                std::cerr << "Lua error when building GUI: " << error << std::endl;
            else
                std::cerr << "Lua error when building GUI but no error message was available" << std::endl;
        } else {
            lua_getglobal(state, "inferred");
            if(!lua_isnil(state, -1)) {
                typeInferInfo = ParseTypeInferInfo(state);
            }
            lua_pop(state, 1);

            lua_getglobal(state, "layout");
            if(!lua_isnil(state, -1)) {
                countTime.Start();
                int widgetCount = CountElements(state);

                lua_getglobal(state, "preparse_layouts");
                int offset = 0;
                if(!lua_isnil(state, -1)) {
                    lua_pushnil(state);
                    while(lua_next(state, -2)) {
                        widgetCount += CountElements(state);
                        lua_pop(state, 1);
                    }

                    widgets.resize(widgetCount);

                    lua_pushnil(state);
                    while(lua_next(state, -2)) {
                        lua_pushstring(state, "name");
                        lua_pushstring(state, lua_tostring(state, -3));
                        lua_settable(state, -3);
                        offset += ParseLayout(state, &widgets[offset]);
                        preparsedLayouts.push_back((Layout*)layoutsStack.back());
                        layoutsStack.pop_back();
                        lua_pop(state, 1);
                    }
                } else {
                    widgets.resize(widgetCount);
                }
                lua_pop(state, 1);
                countTime.Stop();

                parseTime.Start();
                // Change this at some point
                ParseLayout(state, widgets.data() + offset);
                lua_pop(state, 1);
                parseTime.Stop();

                if(layoutsStack.empty())
                    return;

                rootLayout = (Layout*)layoutsStack.back();
                layoutsStack.pop_back();

                rootLayout->bounds = { 0.0f, 0.0f, (float)resolutionX, (float)resolutionY };
                buildLayoutsTime.Start();
                BuildLayouts(rootLayout);
                buildLayoutsTime.Stop();
            } else {
                std::cerr << "No layout widget found" << std::endl;
            }

            timer.Stop();
            printf("GUI build time: %f\n\tDestroy: %f\n\tLua: %f\n\tCount: %f\n\tParse: %f\n\tBuild layouts: %f\n\tTotalBuild: %f\n"
                        , timer.GetTimeMillisecondsFraction()
                        , destroyTime.GetTimeMillisecondsFraction()
                        , luaTime.GetTimeMillisecondsFraction()
                        , countTime.GetTimeMillisecondsFraction()
                        , parseTime.GetTimeMillisecondsFraction()
                        , buildLayoutsTime.GetTimeMillisecondsFraction()
                        , timer.GetTimeMillisecondsFraction() - destroyTime.GetTimeMillisecondsFraction());
        }
    }

    int32_t GetDrawListCount()
    {
        return drawLists.size();
    }

    const DrawList* GetDrawLists()
    {
        return drawLists.data();
    }

    void BuildGUI(lua_State* state, const char* path)
    {
        if(inotifyHandle == -1) {
            inotifyHandle = inotify_init();
            if(inotifyHandle < 0) {
                std::cerr << "Couldn't initialize inotify, automatic reloading disabled" << std::endl;
                autoReload = false;
            }
            if(autoReload) {
                char tempPath[256];
                for(size_t i = 0; i < extensions.size(); ++i) {
                    strcpy(&tempPath[0], &extensions[i].path[0]);
                    const char* slash = std::strrchr(tempPath, '/');
                    if(slash != nullptr) {
                        tempPath[slash - tempPath] = '\0';
                        inotify_add_watch(inotifyHandle, &tempPath[0], IN_CLOSE_WRITE);
                    }
                }
                strcpy(&tempPath[0], path);
                tempPath[std::strrchr(tempPath, '/') - tempPath] = '\0';
                inotify_add_watch(inotifyHandle, &tempPath[0], IN_MODIFY);

#ifdef NDEBUG
                if(getcwd(tempPath, 256))
                    inotify_add_watch(inotifyHandle, &tempPath[0], IN_CLOSE_WRITE);
#else
                    inotify_add_watch(inotifyHandle, "bin", IN_CLOSE_WRITE); // TODO: Change this?
#endif

                strcpy(&sourcePath[0], path);
            }
        }

        
        BuildGUI(state);
    }

    void ReloadGUI(lua_State* state)
    {
        struct pollfd fds;
        fds.fd = inotifyHandle;
        fds.events = POLLIN;

        int length = poll(&fds, 1, 0);
        if(length > 0) {
            // Just reload extensions and all
            char buffer[1024];
            while(read(inotifyHandle, &buffer[0], 1024) == 1024) {}

            BuildGUI(state);
        } else if(length == -1) {
            std::cerr << "inotify poll returned -1" << std::endl;
        }
    }

    void RegisterSharedLibrary(const char* name, const char* path)
    {
        void* lib = dlopen(path, RTLD_NOW);
        if(lib == nullptr) {
            std::cerr << "Error when calling dlopen: " << dlerror() << std::endl;
            return;
        }

        Extension extension;
        extension.initFunction = (InitFunction)dlsym(lib, "Init");
        extension.destroyFunction = (DestroyFunction)dlsym(lib, "Destroy");
        extension.countFunction = (CountFunction)dlsym(lib, "Count");
        extension.measureFunction = (MeasureFunction)dlsym(lib, "Measure");
        extension.parseLayoutFunction = (ParseLayoutFunction)dlsym(lib, "ParseLayout");
        extension.parseWidgetFunction = (ParseWidgetFunction)dlsym(lib, "ParseWidget");
        extension.buildLayoutFunction = (BuildLayoutFunction)dlsym(lib, "BuildLayout");
        extension.buildWidgetFunction = (BuildWidgetFunction)dlsym(lib, "BuildWidget");
        extension.buildChildrenFunction = (BuildChildrenFunction)dlsym(lib, "BuildChildren");
        extension.onEnterFunction = (OnEnterFunction)dlsym(lib, "OnEnter");
        extension.onExitFunction = (OnExitFunction)dlsym(lib, "OnExit");
        extension.onClickFunction = (OnClickFunction)dlsym(lib, "OnClick");
        extension.onReleaseInsideFunction = (OnReleaseInsideFunction)dlsym(lib, "OnReleaseInside");
        extension.onReleaseOutsideFunction = (OnReleaseOutsideFunction)dlsym(lib, "OnReleaseOutside");
        extension.onScrollFunction = (OnScrollFunction)dlsym(lib, "OnScroll");
        extension.onUpdateFunction = (OnUpdateFunction)dlsym(lib, "OnUpdate");
        extension.onChildClickedFunction = (OnChildClickedFunction)dlsym(lib, "OnChildClicked");
        extension.onChildReleasedFunction = (OnChildReleasedFunction)dlsym(lib, "OnChildReleased");
        extension.onChildUpdateFunction = (OnChildUpdateFunction)dlsym(lib, "OnChildUpdate");
        extension.onPopupClosedFunction = (OnPopupClosedFunction)dlsym(lib, "OnPopupClosed");
        extension.queryNumberFunction = (QueryNumberFunction)dlsym(lib, "QueryNumber");
        extension.queryStringFunction = (QueryStringFunction)dlsym(lib, "QueryString");
        extension.setNumberFunction = (SetNumberFunction)dlsym(lib, "SetNumber");
        extension.setStringFunction = (SetStringFunction)dlsym(lib, "SetString");

        // TODO: More error checking
        if(!extension.parseLayoutFunction && !extension.parseWidgetFunction) {
            std::cerr << "Cannot register an extension without a ParseLayout or ParseWidget function";
            dlclose(lib);
        } else if(extension.parseLayoutFunction && extension.parseWidgetFunction) {
            std::cerr << "Cannot register an extension with both a ParseLayout and ParseWidget function";
            dlclose(lib);
        } else {
            std::strcpy(&extension.name[0], name);
            extension.libraryHandle = lib;
            extensions.push_back(extension);
            std::strcpy(&extensions.back().path[0], path);

            if(extension.initFunction)
                extension.initFunction(GetFontHeight(), &initFunctions);
        }
    }

    void UnregisterSharedLibrary(const char* name)
    {
        for(size_t i = 0; i < extensions.size(); ++i) {
            if(streq(extensions[i].name, name)) {
                dlclose(extensions[i].path);
                extensions.erase(extensions.begin() + i);
                return;
            }
        }
    }

    std::vector<uint8_t> fontImageData;
    static uint32_t fontImageWidth = 512;
    static uint32_t fontImageHeight = 512;
    const uint8_t* GetFontTextureData()
    {
        return fontImageData.data();
    }

    int32_t GetFontTextureWidth()
    {
        return fontImageWidth;
    }

    int32_t GetFontTextureHeight()
    {
        return fontImageHeight;
    }

    void InitGUI(size_t resolutionX, size_t resolutionY)
    {
        GUI::resolutionX = resolutionX;
        GUI::resolutionY = resolutionY;

        fontImageData.resize(512 * 512 * 4, 0);
        CreateFont("content/UbuntuMono-R.ttf", fontImageData.data(), fontImageWidth, fontImageHeight);
    }

    void ResolutionChanged(lua_State* state, int32_t width, int32_t height)
    {
        GUI::resolutionX = width;
        GUI::resolutionY = height;

        BuildGUI(state);
    }

    void MouseDown(lua_State* state, int32_t x, int32_t y)
    {
        mouseDown = true;
        int32_t widgetLayer = 0;

        Widget* newDownWidget = nullptr;

        if(popups.empty()) {
            for(int32_t i = 0; i < (int32_t)widgets.size(); ++i) {
                Widget& widget = widgets[i];
                if(widget.draw) {
                    const Rect bounds = widget.bounds;
                    const Rect clipRect = UnpackClipRect(widget.clipRect);
                    if(bounds.Contains(x, y)
                        && (!clipRect.Nonzero() || (clipRect.Nonzero() && clipRect.Contains(x, y)))
                        && widgets[i].layer >= widgetLayer)
                    {
                        downWidget = i;
                        widgetLayer = widgets[i].layer;
                        newDownWidget = &widget;
                    }
                }
            }
        } else {
            for(int32_t i = 0; i < (int32_t)widgets.size(); ++i) {
                Widget* widget = &widgets[i];
                if(widget->draw) {
                    const Rect bounds = widget->bounds;
                    const Rect clipRect = UnpackClipRect(widget->clipRect);
                    if(widget->mask == popups.back().widgetMask
                        && (!clipRect.Nonzero() || (clipRect.Nonzero() && clipRect.Contains(x, y)))
                        && bounds.Contains(x, y))
                    {
                        popups.back().downWidget = i;
                        newDownWidget = widget;
                    }
                }
            }
        }

        if(newDownWidget) {
            if(extensions[newDownWidget->extension].onClickFunction) {
                Element* parent = newDownWidget->parent;
                if(extensions[newDownWidget->extension].onClickFunction(newDownWidget, state, x, y))
                {
                    while(parent) {
                        if(extensions[parent->extension].onChildClickedFunction
                            && extensions[parent->extension].onChildClickedFunction(parent, state, newDownWidget, x, y))
                        {
                            break;
                        }
                        parent = parent->parent;
                    }
                }
            }
        } else {
            if(!popups.empty() && popups.back().closeOn == CLICK)
                ClosePopup(state);
        }
    }

    void Scroll(lua_State* state, int32_t mouseX, int32_t mouseY, int32_t scrollX, int32_t scrollY)
    {
        int32_t hoverWidget = -1;
        if(popups.empty()) {
            int32_t widgetLayer = 0;
            for(int32_t i = 0; i < (int32_t)widgets.size(); ++i) {
                Widget& widget = widgets[i];
                if(widget.draw) {
                    const Rect bounds = widget.bounds;
                    const Rect clipRect = UnpackClipRect(widget.clipRect);
                    if(bounds.Contains(mouseX, mouseY)
                        && (!clipRect.Nonzero() || (clipRect.Nonzero() && clipRect.Contains(mouseX, mouseY)))
                        && widgets[i].layer >= widgetLayer)
                    {
                        hoverWidget = i;
                        widgetLayer = widgets[i].layer;
                    }
                }
            }
        } else {
            for(int32_t i = 0; i < (int32_t)widgets.size(); ++i) {
                Widget* widget = &widgets[i];
                if(widget->draw) {
                    const Rect bounds = widget->bounds;
                    const Rect clipRect = UnpackClipRect(widget->clipRect);
                    if(widget->mask == popups.back().widgetMask
                        && (!clipRect.Nonzero() || (clipRect.Nonzero() && clipRect.Contains(mouseX, mouseY)))
                        && bounds.Contains(mouseX, mouseY))
                    {
                        hoverWidget = i;
                    }
                }
            }
        }

        if(hoverWidget != -1) {
            Element* element = &widgets[hoverWidget];
            while(element)
            {
                if(extensions[element->extension].onScrollFunction
                    && extensions[element->extension].onScrollFunction(element, state, scrollX, scrollY))
                {
                    break;
                }
                element = element->parent;
            }
        }
    }

    void MouseUp(lua_State* state, int x, int y)
    {
        mouseDown = false;
        Widget* widget = nullptr;
        int32_t* downWidgetPtr = nullptr;
        bool sendClickEvent = false;

        if(popups.empty()) {
            if(downWidget != -1) {
                widget = &widgets[downWidget];
                downWidgetPtr = &downWidget;
            }
            // Do nothing if releasing over non-clicked widget
        } else {
            downWidgetPtr = &popups.back().downWidget;
            if(*downWidgetPtr != -1) {
                // Widget in top popup was clicked
                widget = &widgets[popups.back().downWidget];
            } else {
                if(popups.back().hoveredWidget != -1) {
                    // Widget in top popup was hovered, but never clicked, send both click and release events
                    widget = &widgets[popups.back().hoveredWidget];
                    downWidgetPtr = &popups.back().hoveredWidget;
                    sendClickEvent = true;
                } else if(!popupOpened) { // Don't do anything if a popup was opened while mouse was down
                    if(popups.size() == 1) {
                        if(downWidget) {
                            // No widget in top popup was clicked or hovered, but some none-popup widget has been clicked
                            widget = &widgets[downWidget];
                            downWidgetPtr = &downWidget;
                        }
                    } else {
                        Popup& popup = popups[popups.size() - 2];
                        if(popup.downWidget) {
                            // No widget in top popup was clicked or hovered, but some widget was clicked in second-top
                            widget = &widgets[popup.downWidget];
                            downWidgetPtr = &popup.downWidget;
                        }
                    }
                }
            }
        } 

        if(downWidgetPtr && *downWidgetPtr != -1) {
            Element* parent = widget->parent;
            if(sendClickEvent && extensions[widget->extension].onClickFunction) {
                extensions[widget->extension].onClickFunction(widget, state, x, y);
            }
            if(widget->bounds.Contains(x, y)) {
                if(extensions[widget->extension].onReleaseInsideFunction) {
                    if(extensions[widget->extension].onReleaseInsideFunction(widget, state, x, y)) {
                        while(parent) {
                            if(extensions[parent->extension].onChildReleasedFunction
                                 && extensions[parent->extension].onChildReleasedFunction(parent, state, widget, x, y))
                            {
                                break;
                            }
                            parent = parent->parent;
                        }
                    }
                }
            } else {
                if(extensions[widget->extension].onReleaseOutsideFunction) {
                    if(extensions[widget->extension].onReleaseOutsideFunction(widget, state, x, y)) {
                        while(parent) {
                            if(extensions[parent->extension].onChildReleasedFunction
                                && extensions[parent->extension].onChildReleasedFunction(parent, state, widget, x, y))
                            {
                                break;
                            }
                            parent = parent->parent;
                        }
                    }
                }
            }
            if(!sendClickEvent)
                *downWidgetPtr = -1;
        } else {
            
        }

        popupOpened = false;
    }

    void UpdateWidgets(lua_State* state, int32_t x, int32_t y, Widget* widgets, int32_t widgetCount, int32_t* hoveredWidget, int32_t* widgetMask)
    {
        int32_t layer = 0;
        int32_t newHoveredWidget = -1;

        if(mouseOwnElement)
            return;

        for(int32_t i = 0; i < widgetCount; ++i) {
            Widget& widget = widgets[i];
            if(widget.draw) {
                if(widgetMask && widget.mask != *widgetMask)
                    continue;

                Rect bounds = widget.bounds;
                Rect clipRect = UnpackClipRect(widget.clipRect);
                if(bounds.Contains(x, y) && widget.layer >= layer) {
                    if(clipRect.Nonzero()) {
                        if(clipRect.Contains(x, y)) {
                            newHoveredWidget = i;
                            layer = widget.layer;
                        }
                    } else {
                        newHoveredWidget = i;
                        layer = widget.layer;
                    }
                }
            }
        }

        if(newHoveredWidget != *hoveredWidget)
        { 
            if(*hoveredWidget != -1) {
                Widget& oldWidget = widgets[*hoveredWidget];
                if(oldWidget.extension != -1 && extensions[oldWidget.extension].onExitFunction)
                    extensions[oldWidget.extension].onExitFunction(&oldWidget, state);
            }

            *hoveredWidget = newHoveredWidget;

            if(newHoveredWidget != -1) {
                Widget& widget = widgets[newHoveredWidget];
                if(widget.extension != -1 && extensions[widget.extension].onEnterFunction)
                    extensions[widget.extension].onEnterFunction(&widget, state);
            }
        }
    }

    void UpdateDrawList(DrawList& drawList, Vertex* vertices, uint32_t* indicies, Widget* widgets, size_t widgetCount, int32_t layer, uint64_t clipRect, size_t* vertexOffset, size_t* indexOffset)
    {
        size_t vertexCount = 0;
        size_t indexCount = 0;

        for(size_t i = 0; i < widgetCount; ++i) {
            Widget& widget = widgets[i];

            if(widget.draw && widget.layer == layer && widget.clipRect == clipRect) { 
                std::memcpy(vertices + vertexCount, widget.vertices, sizeof(Vertex) * widget.vertexCount);
                std::memcpy(indicies + indexCount, widget.indicies, sizeof(uint32_t) * widget.indexCount);

                for(int32_t j = 0; j < widget.indexCount; ++j) {
                    indicies[indexCount + j] += vertexCount + *vertexOffset;
                } 
                widget.modified = false;

                vertexCount += widget.vertexCount;
                indexCount += widget.indexCount;
            }
        }

        drawList.vertices = vertices;
        drawList.vertexCount = vertexCount;
        drawList.indicies = indicies;
        drawList.indexCount = indexCount;

        *vertexOffset += vertexCount;
        *indexOffset += indexCount;
    }

    void UpdateGUI(lua_State* state, int32_t x, int32_t y)
    {
        if(!widgets.empty()) {
            if(!popups.empty()) {
                int32_t popupsToPop = 0;
                for(int32_t i = popups.size() - 1; i >= 0; --i) {
                    Popup& popup = popups[i];

                    if(popup.closeOn != HOVER) {
                        break;
                    }

                    if(popup.closeOn == HOVER) {
                        bool found = false;
                        for(int32_t j = 0; j < (int32_t)widgets.size(); ++j) {
                            Widget* widget = &widgets[j];
                            if(widget->mask != popup.widgetMask)
                                continue;
                            if(widget->draw && widget->bounds.Contains(x, y)) { // TODO: check only widgets here and in other places
                                Rect clipRect = UnpackClipRect(widget->clipRect);
                                if(clipRect.Nonzero()) {
                                    if(clipRect.Contains(x, y)) {
                                        found = true;
                                        break;
                                    }
                                } else {
                                    found = true;
                                    break;
                                }
                            }
                        }

                        if(!found) {
                            if(widgets[popup.parent].bounds.Contains(x, y)) {
                                break;
                            } else {
                                popupsToPop++;
                            }
                        } else {
                            break;
                        }
                    }
                }

                ClosePopups(state, popupsToPop);
            }

            int32_t* hoveredWidgetPtr;
            if(popups.empty())
                hoveredWidgetPtr = &hoveredWidget;
            else
                hoveredWidgetPtr = &popups.back().hoveredWidget;

            int32_t* widgetMask = nullptr;
            if(!popups.empty())
                widgetMask = &popups.back().widgetMask;

            UpdateWidgets(state, x, y, widgets.data(), widgets.size(), hoveredWidgetPtr, widgetMask);

            for(size_t i = 0; i < widgets.size(); ++i) {
                if(widgets[i].update && extensions[widgets[i].extension].onUpdateFunction) {
                    extensions[widgets[i].extension].onUpdateFunction(&widgets[i], state, x, y);
                    Element* parent = widgets[i].parent;
                    while(parent)
                    {
                        if(extensions[parent->extension].onChildUpdateFunction
                            && extensions[parent->extension].onChildUpdateFunction(parent, state, &widgets[i], x, y))
                        {
                            break;
                        }
                        parent = parent->parent;
                    }
                }
            }

            int vertexCount = 0;
            int indexCount = 0;
            for(size_t i = 0; i < widgets.size(); ++i) {
                if(widgets[i].draw) {
                    vertexCount += widgets[i].vertexCount;
                    indexCount += widgets[i].indexCount;
                }
            }
            if((int)vertices.size() != vertexCount) {
                vertices.resize(vertexCount);
            }
            if((int)indicies.size() != indexCount) {
                indicies.resize(indexCount);
            }

            const static int MAX_CLIP_RECTS = 64; // Max of 64 unique clip rects for now
            struct LayerClipRect
            {
                int32_t layer;
                uint64_t clipRect;
            };
            int layerClipRectCount = 0;
            LayerClipRect layerClipRect[MAX_CLIP_RECTS];

            int32_t layer = 0;
            int32_t nextLayer = 0;
            bool loop = true;
            while(loop) {
                for(size_t i = 0; i < widgets.size(); ++i) {
                    if(widgets[i].layer == layer) {
                        if(layerClipRectCount == 0) {
                            layerClipRect[layerClipRectCount].layer = widgets[i].layer;
                            layerClipRect[layerClipRectCount++].clipRect = widgets[i].clipRect;
                        } else {
                            bool found = false;
                            for(int j = 0; j < layerClipRectCount; ++j) {
                                if(layerClipRect[j].layer == widgets[i].layer
                                    && layerClipRect[j].clipRect == widgets[i].clipRect)
                                {
                                    found = true;
                                    break;
                                }
                            }

                            if(!found) {
                                layerClipRect[layerClipRectCount++] = { widgets[i].layer, widgets[i].clipRect };
                            }
                        }
                    } else {
                        if(widgets[i].layer > layer) {
                            if(nextLayer == layer)
                                nextLayer = widgets[i].layer;
                            else
                                nextLayer = std::min(nextLayer, widgets[i].layer);
                        }
                    }
                }

                if(layer == nextLayer)
                    loop = false;
                layer = nextLayer;
            }

            drawLists.resize(layerClipRectCount);
            size_t vertexOffset = 0;
            size_t indexOffset = 0;
            for(int i = 0; i < layerClipRectCount; ++i) {
                memset(drawLists.data() + i, 0, sizeof(DrawList));
                UpdateDrawList(drawLists[i], vertices.data() + vertexOffset, indicies.data() + indexOffset, widgets.data(), widgets.size(), layerClipRect[i].layer, layerClipRect[i].clipRect, &vertexOffset, &indexOffset);
                drawLists[i].clipRect = UnpackClipRect(layerClipRect[i].clipRect);
            }
        }
    }
}

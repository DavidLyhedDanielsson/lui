#include "glimpl.h"
#include <timer.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>

#if BUILD_SERVER
#include "server/netimpl.h"
#define GUIImpl NetGUI
#else
#include "gui.h"
#define GUIImpl GUI
#endif

static GLint resolutionUniform;
static GLuint fontTexture;

const static char* vertexShaderSource = "\
#version 150\n\
in vec2 position;\
in vec2 uv;\
in vec4 color;\
out vec4 vertexColor;\
out vec2 vertexUV;\
uniform vec2 resolution;\
void main(){\
    vertexColor = color;\
    vec2 pos = position / vec2(resolution);\
    pos.y = 1.0f - pos.y;\
    pos -= vec2(0.5f, 0.5f);\
    pos *= 2.0f;\
    gl_Position = vec4(pos, 0.0f, 1.0f);\
    vertexUV = uv;\
}";

const static char* fragmentShaderSource = "\
#version 150\n\
in vec4 vertexColor;\
in vec2 vertexUV;\
out vec4 fragmentColor;\
uniform sampler2D tex;\
void main(){\
    fragmentColor = texture(tex, vertexUV) * vertexColor;\
}";

static GLuint shaderProgram;
static GLuint vao[2]; //Double buffering
static GLuint vbo[2];
static GLuint ebo[2];
static int vaoIndex = 0;

const static int MAX_QUADS = 1024;
const static int MAX_VERTEX_COUNT = MAX_QUADS * 4;
const static int MAX_INDEX_COUNT = MAX_QUADS * 6;

static int resolutionX = 0;
static int resolutionY = 0;

void GLGUI::InitGUI(size_t resolutionX, size_t resolutionY) {
    GUIImpl::InitGUI(resolutionX, resolutionY);

    ::resolutionX = resolutionX;
    ::resolutionY = resolutionY;

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    GLint status;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
    if(status != GL_TRUE) {
        char buffer[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, buffer);
        std::cerr << buffer << std::endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);

    if(status != GL_TRUE) {
        char buffer[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, buffer);
        std::cerr << buffer << std::endl;
    }

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDetachShader(shaderProgram, vertexShader);
    glDeleteShader(vertexShader);
    glDetachShader(shaderProgram, fragmentShader);
    glDeleteShader(fragmentShader);

    glGenVertexArrays(2, vao);
    glGenBuffers(2, vbo);
    glGenBuffers(2, ebo);
    for(int i = 0; i < 2; ++i) {
        glBindVertexArray(vao[i]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
        glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_COUNT * sizeof(GUI::Vertex), nullptr, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[i]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_INDEX_COUNT * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        GLint position = glGetAttribLocation(shaderProgram, "position");
        glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, sizeof(GUI::Vertex), 0);
        glEnableVertexAttribArray(position);

        GLint uv = glGetAttribLocation(shaderProgram, "uv");
        glVertexAttribPointer(uv, 2, GL_FLOAT, GL_TRUE, sizeof(GUI::Vertex), (void*)(sizeof(float) * 2));
        glEnableVertexAttribArray(uv);

        GLint color = glGetAttribLocation(shaderProgram, "color");
        glVertexAttribPointer(color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GUI::Vertex), (void*)(sizeof(float) * 4));
        glEnableVertexAttribArray(color);
    }

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    resolutionUniform = glGetUniformLocation(shaderProgram, "resolution");

    glUseProgram(shaderProgram);
    glUniform2f(resolutionUniform, resolutionX, resolutionY);
    glUseProgram(0);

    uint32_t imageWidth = GUIImpl::GetFontTextureWidth();
    uint32_t imageHeight = GUIImpl::GetFontTextureHeight();
    const uint8_t* textureData = GUIImpl::GetFontTextureData();

    glGenTextures(1, &fontTexture);
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageWidth, imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLGUI::BuildGUI(lua_State* state, const char* path)
{
    GUIImpl::BuildGUI(state, path);
}

void GLGUI::ReloadGUI(lua_State* state)
{
    GUIImpl::ReloadGUI(state);
}

void GLGUI::DrawGUI()
{
    //Timer drawTimer;
    //drawTimer.Start();

    vaoIndex = (vaoIndex + 1) % 2;

    glBindVertexArray(vao[vaoIndex]);
    glUseProgram(shaderProgram);

    uint32_t drawListCount = GUIImpl::GetDrawListCount();
    const GUI::DrawList* drawLists = GUIImpl::GetDrawLists();

    glBindBuffer(GL_ARRAY_BUFFER, vbo[vaoIndex]);
    void* vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    size_t vertexOffset = 0;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo[vaoIndex]);
    void* indicies = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
    size_t indexOffset = 0;

    for(uint32_t i = 0; i < drawListCount; ++i) {
        assert(vertexOffset + drawLists[i].vertexCount <= MAX_VERTEX_COUNT);
        assert(indexOffset + drawLists[i].indexCount <= MAX_INDEX_COUNT);

        std::memcpy((void*)((GUI::Vertex*)vertices + vertexOffset)
                        , (void*)drawLists[i].vertices
                        , sizeof(GUI::Vertex) * drawLists[i].vertexCount);
        vertexOffset += drawLists[i].vertexCount;

        std::memcpy((void*)((GLuint*)indicies + indexOffset)
                        , (void*)drawLists[i].indicies
                        , sizeof(GLuint) * drawLists[i].indexCount);
        indexOffset += drawLists[i].indexCount;
    }
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    glBindTexture(GL_TEXTURE_2D, fontTexture);
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, resolutionX, resolutionY);

    indexOffset = 0;
    for(uint32_t i = 0; i < drawListCount; ++i) {
        GUI::Rect clipRect = drawLists[i].clipRect;
        if(clipRect.x != 0 || clipRect.y != 0 || clipRect.width != 0 || clipRect.height != 0)
            glScissor((GLint)clipRect.x, (GLint)(resolutionY - clipRect.y - clipRect.height), (GLsizei)clipRect.width, (GLsizei)clipRect.height);
        else
            glScissor(0, 0, resolutionX, resolutionY);

        glDrawElements(GL_TRIANGLES, drawLists[i].indexCount, GL_UNSIGNED_INT, (void*)(indexOffset * sizeof(GLuint)));
        indexOffset += drawLists[i].indexCount;
    }
    glScissor(0, 0, resolutionX, resolutionY);

    //drawTimer.Stop();
    //float drawTime = drawTimer.GetTimeMillisecondsFraction();*/

    //if(showDrawTime || showBuildTime) {
        /*vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
        float yOffset = 24.0f;
        vertexOffset = 0;
        if(showDrawTime) {
            std::string drawTimeString = std::to_string(drawTime);
            auto drawTimeVertices = CreateText(drawTimeString.c_str(), {0.0f, (float)backbufferHeight - yOffset, (float)backbufferWidth, 24.0f});
            std::memcpy(vertices, drawTimeVertices.data(), sizeof(GUI::Vertex) * drawTimeVertices.size());
            vertexOffset += drawTimeVertices.size();
            yOffset += 24.0f;
        }
        if(showBuildTime) {
            std::string buildTimeString = std::to_string(buildTime);
            auto buildTimeVertices = CreateText(buildTimeString.c_str(), {0.0f, (float)backbufferHeight - yOffset, (float)backbufferWidth, 24.0f});
            std::memcpy((void*)((GUI::Vertex*)vertices + vertexOffset), buildTimeVertices.data(), sizeof(GUI::Vertex) * buildTimeVertices.size());
            vertexOffset += buildTimeVertices.size();
            yOffset += 24.0f;
        }
        glUnmapBuffer(GL_ARRAY_BUFFER);
        glDrawArrays(GL_TRIANGLES, 0, vertexOffset);*/
    //}
}

void GLGUI::DestroyGUI(lua_State* state)
{
    glUseProgram(0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(2, vao);
    glDeleteBuffers(2, vbo);
    glDeleteTextures(1, &fontTexture);

    GUIImpl::DestroyGUI(state);
}

#include <cassert>
void GLGUI::ResolutionChanged(lua_State* state, int32_t width, int32_t height)
{
    GUIImpl::ResolutionChanged(state, width, height);

    resolutionX = width;
    resolutionY = height;

    glUseProgram(shaderProgram);
    glUniform2f(resolutionUniform, (float)width, (float)height);
}

void GLGUI::UpdateGUI(lua_State* state, uint32_t x, uint32_t y)
{
    GUIImpl::UpdateGUI(state, x, y);
}

void GLGUI::MouseDown(lua_State* state, int32_t x, int32_t y)
{
    GUIImpl::MouseDown(state, x, y);
}

void GLGUI::MouseUp(lua_State* state, int32_t x, int32_t y)
{
    GUIImpl::MouseUp(state, x, y);
}

void GLGUI::Scroll(lua_State* state, int32_t mouseX, int32_t mouseY, int32_t scrollX, int32_t scrollY)
{
    GUIImpl::Scroll(state, mouseX, mouseY, scrollX, scrollY);
}

void GLGUI::RegisterSharedLibrary(const char* name, const char* path)
{
    GUIImpl::RegisterSharedLibrary(name, path);
}
#include "netimpl.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <cassert>
#include "shared.h"

#define WAIT_AND_RETURN int status; waitpid(pid, &status, 0); pid = -1; return false;

static pid_t pid = -1;
static int linkid[2];
static int socketfd = -1;
static int clientSocket;

const static char* SERVER_ADDRESS = "luiserver";

bool connected = false;
bool tryReconnect = true;

bool ReconnectAndReinit();

void ReadPipe()
{
    if(!connected) // Failsafe
        return;

    pollfd fds;
    fds.fd = linkid[0];
    fds.events = POLLIN;

    while(poll(&fds, 1, 0) > 0) {
        char buf[257];
        auto bytes = read(linkid[0], buf, 256);
        assert(bytes != -1);
        buf[bytes] = '\0';
        std::cout << buf << std::flush;
    }
}

bool Send(const char* buffer, size_t bufferSize)
{
    if(clientSocket == -1)
        return false;

    ssize_t sent = send(clientSocket, buffer, bufferSize, 0);
    if(sent <= 0) {
        if(sent == -1) {
            std::cerr << "Couldn't send data \"" << buffer << "\", errono: " << errno << " " << strerror(errno) << std::endl;
        } else if(sent == 0) {
            std::cerr << "Server shut down" << std::endl;
        } else {
            std::cerr << "send returned " << sent << " when sending \"" << buffer << "\"" << std::endl;
        }
        return false;
    }

    if((size_t)sent != bufferSize) {
        std::cerr << "Didn't send entire buffer, " << sent << " != " << bufferSize << std::endl;
    }

    return true;
}

bool Recv(char* buffer, size_t maxSize, ssize_t* outBytes = nullptr)
{
    if(clientSocket == -1)
        return false;

    ssize_t bytes = recv(clientSocket, buffer, maxSize, 0); 
    if(bytes <= 0) {
        if(bytes == -1) {
            std::cerr << "Couldn't receive data, errono: " << errno << " " << strerror(errno) << std::endl;
        } else if(bytes == 0) {
            std::cerr << "Server shut down" << std::endl;
        } else {
            std::cerr << "recv returned " << bytes << std::endl;
        }
        return false;
    }

    if(outBytes)
        *outBytes = bytes;
    return true;
}

bool ConnectToServer()
{
    if(pipe(linkid) == -1) {
        std::cerr << "Pipe failed" << std::endl;
        return false;
    }
#ifndef DONT_FORK
    if(pid == -1) {
        pid = fork();
        if(pid == 0) {
            dup2(linkid[1], STDOUT_FILENO);
            close(linkid[0]);
            close(linkid[1]);
            if(execl("./bin/server", "/server", (char*)0) == -1) {
                std::cout << "execl failed" << std::endl;
            }
            return 0;
        } else if (pid == -1) {
            std::cerr << "fork failed" << std::endl;
            return -1;
        }

        //close(linkid[1]);
    }
#else
    pid = -1;
#endif

    socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(socketfd == -1) {
        std::cerr << "socket failed: " << strerror(errno) << std::endl;
        WAIT_AND_RETURN;
    }

    struct sockaddr_un serverAddr;
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, SERVER_ADDRESS);

    unlink(SERVER_ADDRESS);
    if(bind(socketfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "bind failed: " << strerror(errno) << std::endl;
        WAIT_AND_RETURN;
    }

    if(listen(socketfd, 1) == -1) {
        std::cerr << "listen failed: " << strerror(errno) << std::endl;
        WAIT_AND_RETURN;
    }

    fd_set rdfs;
    FD_ZERO(&rdfs);
    FD_SET(socketfd, &rdfs);

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    ReadPipe();
    int selectVal = 0;
    selectVal = select(socketfd + 1, &rdfs, NULL, NULL, &tv);
    while(selectVal == 0) {
        ReadPipe();
        std::cout << "Waiting for clients..." << std::endl;
        FD_SET(socketfd, &rdfs);
        tv.tv_sec = 5;
        selectVal = select(socketfd + 1, &rdfs, NULL, NULL, &tv);
    }
    if(selectVal == -1) {
        std::cerr << "select failed: " << strerror(errno) << std::endl;
        WAIT_AND_RETURN;
    }

    struct sockaddr_un clientAddr;
    socklen_t length;
    clientSocket = accept(socketfd, (struct sockaddr*)&clientAddr, &length);
    if(clientSocket == -1) {
        std::cerr << "accept failed: " << strerror(errno) << std::endl;
        WAIT_AND_RETURN;
    }

    if(getpeername(clientSocket, (struct sockaddr*)&clientAddr, &length) == -1) {
        std::cerr << "getpeername failed: " << strerror(errno) << std::endl;
        WAIT_AND_RETURN;
    }
    std::cout << "Client connected: " << clientAddr.sun_path << std::endl;

    ReadPipe();

    connected = true;
    return true;
}

void Close()
{
    close(socketfd);
    socketfd = -1;
    close(clientSocket);
    clientSocket = -1;
    close(linkid[1]);
    connected = false;
}

size_t resolutionX;
size_t resolutionY;
bool NetGUI::InitGUI(size_t resolutionX, size_t resolutionY)
{
    errno = 0;
    if(!ConnectToServer()) {
        return false;
    }

    ::resolutionX = resolutionX;
    ::resolutionY = resolutionY;

    int32_t resolution[2] = { (int32_t)resolutionX, (int32_t)resolutionY };
    if(!Send((char*)resolution, sizeof(resolution))) {
        if(!ReconnectAndReinit())
            return false;
    }

    char buf[128];
    if(!Recv(buf, 128, nullptr)) {
        if(!ReconnectAndReinit())
            return false;
    }

    ReadPipe();

    return true;
}

static std::string path;

void NetGUI::BuildGUI(lua_State* state, const char* path)
{
    ::path = path;
    if(!connected) {
        if(!ReconnectAndReinit())
            return;
    }

    char buffer[1024];
    sprintf(buffer, "%c%s", (char)NetGUI::FUNCTIONS::BuildGUI, path);
    size_t length = strlen(buffer);
    if(!Send(buffer, length + 1)) {
        ReconnectAndReinit();
        return;
    } else if(!Recv(buffer, 1024)) {
        ReconnectAndReinit();
        return;
    }

    ReadPipe();
}

void NetGUI::ReloadGUI(lua_State* state)
{
    if(!connected)
        return;

    char cmd = (char)NetGUI::FUNCTIONS::ReloadGUI;
    if(!Send(&cmd, 1)) {
        ReconnectAndReinit();
        return;
    }
    char buffer[2];
    if(!Recv(buffer, 2)) {
        ReconnectAndReinit();
        return;
    }

    ReadPipe();
}

void NetGUI::DestroyGUI(lua_State* state)
{
    if(connected) {
        char cmd = (char)NetGUI::FUNCTIONS::DestroyGUI;
        char buffer[2];
        if(Send(&cmd, 1)) {
            Recv(buffer, 2);
        }
        ReadPipe();
    }

    Close();
    std::cout << "Waiting for child" << std::endl;
    int status;
    waitpid(pid, &status, 0);
    std::cout << "Waiting done" << std::endl;
}

void NetGUI::ResolutionChanged(lua_State* state, int32_t width, int32_t height)
{
    if(!connected)
        return;

    resolutionX = width;
    resolutionY = height;

    const static auto size = 1 + sizeof(int32_t) * 2;
    char buffer[size];
    buffer[0] = (char)NetGUI::FUNCTIONS::ResolutionChanged;
    memcpy(buffer + 1, &width, sizeof(int32_t));
    memcpy(buffer + 1 + sizeof(int32_t), &height, sizeof(int32_t));
    
    if(!Send(buffer, size)) {
        ReconnectAndReinit();
        return;
    }
    if(!Recv(buffer, 2)) {
        ReconnectAndReinit();
        return;
    }

    ReadPipe();
}

void NetGUI::UpdateGUI(lua_State* state, int32_t x, int32_t y)
{
    if(!connected)
        return;

    const static auto size = 1 + sizeof(int32_t) * 2;
    char buffer[size];
    buffer[0] = (char)NetGUI::FUNCTIONS::UpdateGUI;
    memcpy(buffer + 1, &x, sizeof(int32_t));
    memcpy(buffer + 1 + sizeof(int32_t), &y, sizeof(int32_t));
    
    if(!Send(buffer, size)) {
        ReconnectAndReinit();
        return;
    }
    if(!Recv(buffer, 2)) {
        ReconnectAndReinit();
        return;
    }

    ReadPipe();
}

std::vector<std::pair<std::string, std::string>> libraries;
void NetGUI::RegisterSharedLibrary(const char* name, const char* path)
{
    if(!connected)
        return;
    
    libraries.emplace_back(name, path);

    char buffer[1024];
    size_t size = sprintf(buffer, "%c%s%c%s%c", (char)NetGUI::FUNCTIONS::RegisterSharedLibrary, name, (char)1, path, (char)1);
    if(!Send(buffer, size)) {
        ReconnectAndReinit();
        return;
    }
    if(!Recv(buffer, 1024)) {
        ReconnectAndReinit();
        return;
    }

    ReadPipe();
}

void NetGUI::MouseDown(lua_State* state, int32_t x, int32_t y)
{
    if(!connected)
        return;

    const static auto size = 1 + sizeof(int32_t) * 2;
    char buffer[size];
    buffer[0] = (char)NetGUI::FUNCTIONS::MouseDown;
    memcpy(buffer + 1, &x, sizeof(int32_t));
    memcpy(buffer + 1 + sizeof(int32_t), &y, sizeof(int32_t));

    if(!Send(buffer, size)) {
        ReconnectAndReinit();
        return;
    }
    if(!Recv(buffer, 2)) {
        ReconnectAndReinit();
        return;
    }

    ReadPipe();
}

void NetGUI::MouseUp(lua_State* state, int32_t x, int32_t y)
{
    if(!connected)
        return;

    const static auto size = 1 + sizeof(int32_t) * 2;
    char buffer[size];
    buffer[0] = (char)NetGUI::FUNCTIONS::MouseUp;
    memcpy(buffer + 1, &x, sizeof(int32_t));
    memcpy(buffer + 1 + sizeof(int32_t), &y, sizeof(int32_t));

    if(!Send(buffer, size)) {
        ReconnectAndReinit();
        return;
    }
    if(!Recv(buffer, 2)) {
        ReconnectAndReinit();
        return;
    }

    ReadPipe();
}

void NetGUI::Scroll(lua_State* state, int32_t mouseX, int32_t mouseY, int32_t scrollX, int32_t scrollY)
{
    if(!connected)
        return;

    const static auto size = 1 + sizeof(int32_t) * 4;
    char buffer[size];
    buffer[0] = (char)NetGUI::FUNCTIONS::Scroll;
    int32_t data[4] = { mouseX, mouseY, scrollX, scrollY };
    memcpy(buffer + 1, data, sizeof(int32_t) * 4);

    if(!Send(buffer, size)) {
        ReconnectAndReinit();
        return;
    }
    if(!Recv(buffer, 2)) {
        ReconnectAndReinit();
        return;
    }

    ReadPipe();
}

static std::vector<uint8_t> fontTextureData;
static int32_t fontTextureWidth = -1;
static int32_t fontTextureHeight = -1;
const uint8_t* NetGUI::GetFontTextureData()
{
    if(!connected)
        return nullptr;

    if(fontTextureWidth == -1)
        GetFontTextureWidth();
    if(fontTextureHeight == -1)
        GetFontTextureHeight();

    char cmd = (char)NetGUI::FUNCTIONS::GetFontTextureData;
    if(!Send(&cmd, 1)) {
        ReconnectAndReinit();
        return nullptr;
    }
    size_t dataLength = fontTextureWidth * fontTextureHeight * 4;
    fontTextureData.resize(dataLength);
    size_t writtenLength = 0;
    while(writtenLength < dataLength) {
        ssize_t recvLength;
        if(!Recv((char*)(fontTextureData.data() + writtenLength), dataLength - writtenLength, &recvLength)) {
            ReconnectAndReinit();
            return nullptr;
        }
        writtenLength += recvLength;
    }
    return fontTextureData.data();
}

int32_t NetGUI::GetFontTextureWidth()
{
    if(!connected)
        return -1;

    char cmd = (char)NetGUI::FUNCTIONS::GetFontTextureWidth;
    if(!Send(&cmd, 1)) {
        ReconnectAndReinit();
        return -1;
    }
    if(!Recv((char*)(&fontTextureWidth), sizeof(int32_t))) {
        ReconnectAndReinit();
        return -1;
    }
    ReadPipe();
    return fontTextureWidth;
}

int32_t NetGUI::GetFontTextureHeight()
{
    if(!connected)
        return -1;

    char cmd = (char)NetGUI::FUNCTIONS::GetFontTextureHeight;
    if(!Send(&cmd, 1)) {
        ReconnectAndReinit();
        return -1;
    }
    if(!Recv((char*)(&fontTextureHeight), sizeof(int32_t))) {
        ReconnectAndReinit();
        return -1;
    }
    ReadPipe();
    return fontTextureHeight;
}

std::vector<GUI::Vertex> vertices;
std::vector<uint32_t> indicies;
std::vector<GUI::DrawList> drawLists;
int32_t NetGUI::GetDrawListCount()
{
    if(!connected)
        return 0;

    char cmd = (char)NetGUI::FUNCTIONS::GetDrawListCount;
    if(!Send(&cmd, 1)) {
        ReconnectAndReinit();
        return 0;
    }
    int32_t drawListCount;
    if(!Recv((char*)(&drawListCount), sizeof(int32_t))) {
        ReconnectAndReinit();
        return 0;
    }
    drawLists.resize(drawListCount);
    ReadPipe();
    return drawListCount;
}

const GUI::DrawList* NetGUI::GetDrawLists()
{
    if(!connected)
        return nullptr;

    char cmd = (char)NetGUI::FUNCTIONS::GetDrawLists;
    if(!Send(&cmd, 1)) {
        ReconnectAndReinit();
        return nullptr;
    }
    int32_t drawListCount = 0;
    if(!Recv((char*)&drawListCount, sizeof(drawListCount))) {
        ReconnectAndReinit();
        return nullptr;
    }
    if(!Send("y", 2)) {
        ReconnectAndReinit();
        return nullptr;
    }
    vertices.resize(0);
    indicies.resize(0);
    for(int i = 0; i < drawListCount; ++i) {
        size_t verticesOffset = vertices.size();
        size_t indiciesOffset = indicies.size();

        Header header;
        if(!Recv((char*)&header, sizeof(Header))) {
            ReconnectAndReinit();
            return nullptr;
        }
        if(!Send("y", 2)) {
            ReconnectAndReinit();
            return nullptr;
        }

        if(header.vertexCount > 0) {
            vertices.resize(vertices.size() + header.vertexCount);
            size_t remainingData = sizeof(GUI::Vertex) * header.vertexCount;
            size_t writtenData = 0;
            while(remainingData > 0) {
                ssize_t newData;
                if(!Recv(((char*)&vertices[verticesOffset]) + writtenData, remainingData, &newData)) {
                    ReconnectAndReinit();
                    return nullptr;
                }
                remainingData -= newData;
                writtenData += newData;
            }

            if(!Send("y", 2)) {
                ReconnectAndReinit();
                return nullptr;
            }

            ReadPipe();
        }

        if(header.indexCount > 0) {
            indicies.resize(indicies.size() + header.indexCount);
            size_t remainingData = sizeof(uint32_t) * header.indexCount;
            size_t writtenData = 0;
            while(remainingData > 0) {
                ssize_t newData;
                if(!Recv(((char*)&indicies[indiciesOffset]) + writtenData, remainingData, &newData)) {
                    ReconnectAndReinit();
                    return nullptr;
                }
                remainingData -= newData;
                writtenData += newData;
            }

            if(!Send("y", 2)) {
                ReconnectAndReinit();
                return nullptr;
            }

            ReadPipe();
        }

        drawLists[i].vertexCount = header.vertexCount;
        drawLists[i].indexCount = header.indexCount;
        drawLists[i].vertices = vertices.data() + verticesOffset;
        drawLists[i].indicies = indicies.data() + indiciesOffset;
        drawLists[i].textureIndex = header.textureIndex;
        drawLists[i].clipRect = header.clipRect;
    }

    return drawLists.data();
}

static bool reconnecting = false;
bool ReconnectAndReinit()
{
    if(tryReconnect) {
        tryReconnect = false;
        pid = -1;
        Close();
        if(!ConnectToServer()) {
            return false;
        }

        int32_t resolution[2] = { (int32_t)resolutionX, (int32_t)resolutionY };
        int bytes = send(clientSocket, (char*)resolution, sizeof(resolution), 0);
        if(bytes == -1) {
            std::cerr << "send failed" << std::endl;
            WAIT_AND_RETURN;
        }

        char buf[128];
        int rc = recv(clientSocket, buf, 128, 0);
        assert(rc != -1);

        ReadPipe();

        auto libraries_copy = libraries;
        libraries.clear();
        for(const auto& pair : libraries_copy) {
            NetGUI::RegisterSharedLibrary(pair.first.c_str(), pair.second.c_str());
        }

        if(!reconnecting) {
            reconnecting = true;
            NetGUI::BuildGUI(nullptr, path.c_str());
        } else {
            reconnecting = false;
            return false; // Recursive call to this function, only try to reconnect once
        }
        reconnecting = false;
        tryReconnect = true;
        return true;
    } else {
        Close();
        return false;
    }
}
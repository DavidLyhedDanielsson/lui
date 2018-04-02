/*
 * A server which listens for GUI commands, runs them, and returns the result
 * to the client.
*/
#include "../gui.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <iostream>
#include <cassert>

#include "shared.h"

const static char* CLIENT_ADDRESS = "luiclient";
const static char* SERVER_ADDRESS = "luiserver";

#ifndef DONT_FORK
const static int MAX_CONNECTION_TRIES = 15;
#else
// The application doesn't automatically start the server, so simply
// wait forever so the developer has time to start the client
const static int MAX_CONNECTION_TRIES = -1;
#endif

bool Send(int socket, const char* buffer, size_t bufferSize, bool& run)
{
    ssize_t sent = send(socket, buffer, bufferSize, 0);
    if(sent <= 0) {
        if(sent == -1) {
            std::cerr << "Couldn't send data \"" << buffer << "\", errono: " << errno << " " << strerror(errno) << std::endl;
        } else {
            std::cerr << "send returned " << sent << " when sending \"" << buffer << "\"" << std::endl;
        }
        run = false;
        return false;
    }

    if((size_t)sent != bufferSize) {
        std::cerr << "Didn't send entire buffer, " << sent << " != " << bufferSize << std::endl;
    }

    return true;
}

bool Recv(int socket, char* buffer, size_t maxSize, ssize_t* outBytes, bool& run)
{
    ssize_t bytes = recv(socket, buffer, maxSize, 0); 
    if(bytes <= 0) {
        if(bytes == -1) {
            std::cerr << "Couldn't receive data, errono: " << errno << " " << strerror(errno) << std::endl;
        } else {
            std::cerr << "recv returned " << bytes << std::endl;
        }
        run = false;
        return false;
    }

    if(outBytes)
        *outBytes = bytes;
    return true;
}

bool memstreq(const char* lhs, const char* rhs)
{
    return memcmp(lhs, rhs, strlen(rhs)) == 0;
}

int main(int argc, const char** argv)
{
    int socketfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(socketfd == -1) {
        std::cout << "socket failed" << std::endl;
        return -1;
    }

    sockaddr_un clientAddr;
    clientAddr.sun_family = AF_UNIX;
    strcpy(clientAddr.sun_path, CLIENT_ADDRESS);
    unlink(CLIENT_ADDRESS);
    if(bind(socketfd, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) == -1) {
        std::cout << "bind failed" << std::endl;
        return -1;
    }

    sockaddr_un serverAddr;
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, SERVER_ADDRESS);
    int tries = 0;
    while(tries < MAX_CONNECTION_TRIES || MAX_CONNECTION_TRIES == -1) {
        if(connect(socketfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
            tries++;
            sleep(1);
        } else {
            break;
        }
    }
    if(tries == MAX_CONNECTION_TRIES) {
        std::cout << "connect failed" << std::endl;
        return -1;
    }

    int32_t resolution[2];
    int rc = recv(socketfd, (char*)resolution, sizeof(int32_t) * 2, 0);
    if(rc == -1) {
        std::cout << "Couldn't get resolution from server" << std::endl;
        return -1;
    }

    std::cout << "Got resolution: " << resolution[0] << "x" << resolution[1] << std::endl;
    GUI::InitGUI(resolution[0], resolution[1]);

    lua_State* luaState = luaL_newstate();
    luaL_openlibs(luaState);
    std::cout << "Lua address: " << luaState << std::endl;

    size_t ptrAddr = (size_t)luaState;
    int bytes = send(socketfd, reinterpret_cast<char*>(&ptrAddr), sizeof(ptrAddr), 0);
    if(bytes == -1) {
        std::cout << "Couldn't send lua state" << std::endl;
        return -1;
    }
    assert(bytes == sizeof(ptrAddr));

    int32_t fontTextureWidth = GUI::GetFontTextureWidth();
    std::cout << "fontTextureWidth: " << fontTextureWidth << std::endl;
    int32_t fontTextureHeight = GUI::GetFontTextureHeight();
    std::cout << "fontTextureHeight: " << fontTextureHeight << std::endl;

    bool run = true;
    while(run) {
        char buffer[1024];
        size_t length = recv(socketfd, buffer, 1024, 0);
        buffer[length] = 0;
        if(length == 0) {
            run = false;
        } else {
            switch(buffer[0]) {
                case NetGUI::FUNCTIONS::InitGUI:
                    break;
                case NetGUI::FUNCTIONS::BuildGUI:
                    std::cout << "Building GUI..." << std::endl;
                    GUI::BuildGUI(luaState, buffer + 1);
                    std::cout << "Done building GUI" << std::endl;
                    Send(socketfd, "y", 1, run);
                    break;
                case NetGUI::FUNCTIONS::ReloadGUI:
                    GUI::ReloadGUI(luaState);
                    Send(socketfd, "y", 1, run);
                    break;
                case NetGUI::FUNCTIONS::DestroyGUI:{
                    GUI::DestroyGUI(luaState);
                    Send(socketfd, "y", 1, run);
                    break;}
                case NetGUI::FUNCTIONS::ResolutionChanged:{
                    int32_t pos[2];
                    pos[0] = *((int32_t*)(buffer + 1));
                    pos[1] = *((int32_t*)(buffer + 1 + sizeof(int32_t)));
                    GUI::ResolutionChanged(luaState, pos[0], pos[1]);
                    Send(socketfd, "y", 1, run);
                    break;}
                case NetGUI::FUNCTIONS::UpdateGUI:{
                    int32_t pos[2];
                    pos[0] = *((int32_t*)(buffer + 1));
                    pos[1] = *((int32_t*)(buffer + 1 + sizeof(int32_t)));
                    GUI::UpdateGUI(luaState, pos[0], pos[1]);
                    Send(socketfd, "y", 1, run);
                    break;}
                case NetGUI::FUNCTIONS::RegisterSharedLibrary:{
                    bool breakOnNext = false;
                    for(ssize_t i = 1; i < 1024; ++i) {
                        if(buffer[i] == 1) {
                            buffer[i] = 0;
                            if(breakOnNext) {
                                break;
                            }
                            breakOnNext = true;
                        }
                    }
                    const char* name = buffer + 1;
                    const char* path = buffer + strlen(name) + 2;
                    GUI::RegisterSharedLibrary(name, path);
                    std::cout << "Registered shared library " << name << " at " << path << std::endl;
                    Send(socketfd, "y", 1, run);
                    break;}
                case NetGUI::FUNCTIONS::MouseDown:{
                    int32_t pos[2];
                    pos[0] = *((int32_t*)(buffer + 1));
                    pos[1] = *((int32_t*)(buffer + 1 + sizeof(int32_t)));
                    GUI::MouseDown(luaState, pos[0], pos[1]);
                    Send(socketfd, "y", 1, run);
                    break;}
                case NetGUI::FUNCTIONS::MouseUp:{
                    int32_t pos[2];
                    pos[0] = *((int32_t*)(buffer + 1));
                    pos[1] = *((int32_t*)(buffer + 1 + sizeof(int32_t)));
                    GUI::MouseUp(luaState, pos[0], pos[1]);
                    Send(socketfd, "y", 1, run);
                    break;}
                case NetGUI::FUNCTIONS::Scroll:{
                    int32_t data[4];
                    memcpy(data, buffer + 1, sizeof(int32_t) * 4);
                    GUI::Scroll(luaState, data[0], data[1], data[2], data[3]);
                    Send(socketfd, "y", 1, run);
                    break;}
                case NetGUI::FUNCTIONS::GetFontTextureData:
                    Send(socketfd, (const char*)GUI::GetFontTextureData(), sizeof(uint8_t) * fontTextureWidth * fontTextureHeight * 4, run);
                    break;
                case NetGUI::FUNCTIONS::GetFontTextureWidth:
                    Send(socketfd, (char*)&fontTextureWidth, sizeof(int32_t), run);
                    break;
                case NetGUI::FUNCTIONS::GetFontTextureHeight:
                    Send(socketfd, (char*)&fontTextureHeight, sizeof(int32_t), run);
                    break;
                case NetGUI::FUNCTIONS::GetDrawListCount:{
                    int32_t drawListCount = GUI::GetDrawListCount();
                    Send(socketfd, (char*)&drawListCount, sizeof(int32_t), run);
                    break;}
                case NetGUI::FUNCTIONS::GetDrawLists: {
                    const GUI::DrawList* drawLists = GUI::GetDrawLists();
                    int32_t drawListCount = GUI::GetDrawListCount();
                    if(!Send(socketfd, (char*)&drawListCount, sizeof(drawListCount), run))
                        break;
                    char buffer[2];
                    if(!Recv(socketfd, buffer, 2, nullptr, run))
                        break;
                    for(int32_t i = 0; i < GUI::GetDrawListCount(); ++i) {
                        NetGUI::Header header = { drawLists[i].vertexCount, drawLists[i].indexCount, drawLists[i].textureIndex, drawLists[i].clipRect };

                        if(!Send(socketfd, (char*)&header, sizeof(NetGUI::Header), run))
                            break;
                        if(!Recv(socketfd, buffer, 2, nullptr, run))
                            break;

                        if(header.vertexCount > 0) {
                            if(!Send(socketfd, (char*)drawLists[i].vertices, sizeof(GUI::Vertex) * header.vertexCount, run))
                                break;
                            if(!Recv(socketfd, buffer, 2, nullptr, run))
                                break;
                        }

                        if(header.indexCount > 0) {
                            if(!Send(socketfd, (char*)drawLists[i].indicies, sizeof(uint32_t) * header.indexCount, run))
                                break;
                            if(!Recv(socketfd, buffer, 2, nullptr, run))
                                break;
                        }
                    }
                break;}
                default:
                    std::cerr << "No command with ID " << (int)buffer[0] << std::endl;
                    break;
            }
        }
    }

    close(socketfd);
    unlink(CLIENT_ADDRESS);
    unlink(SERVER_ADDRESS);
    return 0;
}
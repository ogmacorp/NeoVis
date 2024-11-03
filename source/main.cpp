// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017-2024 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of NeoVis is licensed to you under the terms described
//  in the NEOVIS_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#include "imgui/imgui.h"
#include "imgui/imgui-SFML.h"
#include "imgui_extra.h"
#include "CSDRVis.h"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <thread>
#include <mutex>
#include <array>
#include <fstream>
#include <iostream>

const int maxStr = 128;

typedef unsigned char field_type;

enum ConnectionStatus {
    disconnected, connected, connecting
};

ConnectionStatus connectionStatus;
std::string addressStr;
std::string portStr;

std::unique_ptr<std::thread> connectThread;
std::unique_ptr<std::thread> receiveThread;

bool stopReceiving = false;

void receiveThreadFunc(sf::TcpSocket* socket);

void endReceiving() {
    if (receiveThread != nullptr) {
        stopReceiving = true;

        receiveThread->join();

        stopReceiving = false;
    }
}

void connectThreadFunc(sf::TcpSocket* socket) {
    connectionStatus = connecting;

    std::string shortAddress = addressStr;

    int addressLength;

    for (addressLength = 0; addressLength < shortAddress.size(); addressLength++) {
        if (shortAddress[addressLength] == ' ' || shortAddress[addressLength] == '\0')
            break;
    }

    shortAddress.resize(addressLength);

    int port;

    try {
        port = std::stoi(portStr);
    }
    catch (std::exception e) {
        std::cout << "Invalid port!";

        connectionStatus = disconnected;

        return;
    }

    std::cout << "Connecting to address: \"" << shortAddress << "\" on port \"" << port << "\" ..." << std::endl;

    sf::Socket::Status status = socket->connect(shortAddress, port, sf::seconds(5.0f));
    
    if (status == sf::Socket::Status::Done) {
        connectionStatus = connected;

        std::cout << "Connection established!" << std::endl;

        if (receiveThread != nullptr)
            endReceiving();

        // Start receiving
        receiveThread.reset(new std::thread(receiveThreadFunc, socket));

        return;
    }
    else {
        connectionStatus = disconnected;

        std::cout << "Connection failed! Reason:" << std::endl;

        switch (status) {
        case sf::Socket::Status::Disconnected:
            std::cout << "Disconnected!" << std::endl;
            break;
        case sf::Socket::Status::Error:
            std::cout << "Error!" << std::endl;
            break;
        case sf::Socket::Status::NotReady:
            std::cout << "Not Ready!" << std::endl;
            break;
        case sf::Socket::Status::Partial:
            std::cout << "Partial!" << std::endl;
            break;
        }

        return;
    }
}

struct Caret {
    sf::Uint16 layer;
    sf::Vector3i pos;

    Caret()
    : layer(0),
    pos(-1, -1, -1)
    {}
};

struct CSDR {
    sf::Uint16 width, height, columnSize;
    std::vector<sf::Uint16> indices;
};

struct Field {
    std::array<char, 64> name;
    sf::Vector3i fieldSize;
    std::vector<field_type> field;
};

struct Network {
    sf::Uint16 numLayers;
    sf::Uint16 numEncs; // Number of layers that are encodeers
    std::vector<CSDR> sdrs;

    std::vector<Field> fields;

    Network()
    : numLayers(0)
    {}
};

Network bufferedNetwork;
Network network;
Caret caret;

bool recv(sf::TcpSocket* socket, void* data, int size) {
    int numReceived = 0;

    while (numReceived < size) {
        size_t received;

        sf::Socket::Status s = socket->receive(&reinterpret_cast<char*>(data)[numReceived], size - numReceived, received);

        if (s != sf::Socket::Status::Done) {
            connectionStatus = disconnected;
            return false;
        }
            
        numReceived += received;
    }

    return true;
}

void receiveThreadFunc(sf::TcpSocket* socket) {
    while (!stopReceiving) {
        if (recv(socket, &bufferedNetwork.numLayers, sizeof(sf::Uint16))) {
            recv(socket, &bufferedNetwork.numEncs, sizeof(sf::Uint16));

            bufferedNetwork.sdrs.resize(bufferedNetwork.numLayers);

            for (int l = 0; l < bufferedNetwork.numLayers; l++) {
                recv(socket, &bufferedNetwork.sdrs[l].width, sizeof(sf::Uint16));
                recv(socket, &bufferedNetwork.sdrs[l].height, sizeof(sf::Uint16));
                recv(socket, &bufferedNetwork.sdrs[l].columnSize, sizeof(sf::Uint16));

                bufferedNetwork.sdrs[l].indices.resize(bufferedNetwork.sdrs[l].width * bufferedNetwork.sdrs[l].height);
                
                recv(socket, bufferedNetwork.sdrs[l].indices.data(), bufferedNetwork.sdrs[l].indices.size() * sizeof(sf::Uint16));
            }

            // Number of fields
            sf::Uint16 numFields;

            recv(socket, &numFields, sizeof(sf::Uint16));
            
            bufferedNetwork.fields.resize(numFields);

            for (int f = 0; f < numFields; f++) {
                recv(socket, &bufferedNetwork.fields[f].name, sizeof(bufferedNetwork.fields[f].name));
                recv(socket, &bufferedNetwork.fields[f].fieldSize, sizeof(sf::Vector3i));
                int totalSize = bufferedNetwork.fields[f].fieldSize.x * bufferedNetwork.fields[f].fieldSize.y * bufferedNetwork.fields[f].fieldSize.z;

                bufferedNetwork.fields[f].field.resize(totalSize);
                recv(socket, bufferedNetwork.fields[f].field.data(), bufferedNetwork.fields[f].field.size() * sizeof(field_type));
            }
        }
    }
}

int main() {
    sf::RenderWindow window;
    
    window.create(sf::VideoMode(1280, 720), "NeoVis", sf::Style::Default);

    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    ImGui::SFML::Init(window);

    window.resetGLStates();

    //sf::Texture logoTex;
    //logoTex.loadFromFile("resources/logo.png");
    //logoTex.setSmooth(true);

    // ---------------------------- State ---------------------------

    bool connectionWizardOpen = false;

    // Read address and port
    std::ifstream fromConfig("config.txt");

    if (!fromConfig.is_open()) {
        std::cout << "Could not open config file!" << std::endl;

        return 0;
    }

    std::getline(fromConfig, addressStr);
    std::getline(fromConfig, portStr);

    addressStr.resize(maxStr);
    portStr.resize(maxStr);

    std::vector<CSDRVis> layerCSDRVis;
    std::vector<sf::Texture> fieldTextures;
    std::vector<int> fieldZs;

    sf::TcpSocket socket;

    // ---------------------------- Loop ----------------------------

    sf::Clock deltaClock;

    while (window.isOpen()) {
        sf::Event event;

        int mouseWheelDelta = 0;

        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            switch (event.type) {
                case sf::Event::Closed:
                    window.close();
                    break;

                case sf::Event::MouseWheelMoved:
                    mouseWheelDelta = event.mouseWheel.delta;
                    break;
            }
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Connection")) {
                if (ImGui::MenuItem("Connect...")) {
                    connectionWizardOpen = true;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (connectionWizardOpen) {
            bool open = true;

            if (ImGui::Begin("Connection Wizard", &open)) {
                ImGui::InputText("Address", &addressStr[0], addressStr.size());
                
                ImGui::NewLine();
                
                ImGui::InputText("Port", &portStr[0], portStr.size());

                ImGui::NewLine();

                std::string statusStr;
                
                switch (connectionStatus) {
                case connected:
                    statusStr = "Connected!";
                    break;
                case connecting:
                    statusStr = "Connecting...";
                    break;
                case disconnected:
                    statusStr = "Disconnected.";
                    break;
                }

                if (ImGui::Button("Connect!")) {
                    if (connectThread != nullptr)
                        connectThread->join();

                    connectThread.reset(new std::thread(connectThreadFunc, &socket));
                    
                    // Save configuration
                    std::ofstream toConfig("config.txt");

                    toConfig << addressStr << std::endl;
                    toConfig << portStr << std::endl;
                }

                ImGui::LabelText("Status", statusStr.c_str());

                ImGui::End();
            }
            
            if (!open)
                connectionWizardOpen = false;
        }

        if (connectionStatus == disconnected) {
            layerCSDRVis.clear();
            fieldTextures.clear();
        }
        else if (connectionStatus == connected) {
            network = bufferedNetwork;

            // Send Caret
            socket.send(&caret, sizeof(Caret));

            // Init
            if (layerCSDRVis.empty()) {
                layerCSDRVis.resize(network.numLayers);

                for (int l = 0; l < network.numLayers; l++)
                    layerCSDRVis[l].init(network.sdrs[l].width, network.sdrs[l].height, network.sdrs[l].columnSize);
            }

            // Visualize content
            for (int l = 0; l < network.numLayers; l++) {
                CSDR &sdr = network.sdrs[l];

                for (int i = 0; i < sdr.indices.size(); i++)
                    layerCSDRVis[l][i] = sdr.indices[i];

                layerCSDRVis[l].draw();

                if (l < network.numEncs)
                    ImGui::Begin(("Pre-encoder " + std::to_string(l)).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                else
                    ImGui::Begin(("Layer " + std::to_string(l - network.numEncs)).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);

                bool hovering;
                int hoverX = -1;
                int hoverY = -1;

                ImGui::ImageHover(layerCSDRVis[l].getTexture(), hovering, hoverX, hoverY, ImVec2(layerCSDRVis[l].getTexture().getSize().x, layerCSDRVis[l].getTexture().getSize().y));

                if (hovering) {
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
                        // Divide by size
                        layerCSDRVis[l].highlightX = static_cast<int>(hoverX / layerCSDRVis[l].nodeSpaceSize);
                        layerCSDRVis[l].highlightY = layerCSDRVis[l].getSizeInNodes().y - static_cast<int>(hoverY / layerCSDRVis[l].nodeSpaceSize + 1.0f);

                        caret.pos = layerCSDRVis[l].getHighlightedCSDRPos();
                        caret.layer = l;
                    }
                }
                else {
                    layerCSDRVis[l].highlightX = -1;
                    layerCSDRVis[l].highlightY = -1;
                }
    
                ImGui::End();
            }

            // Show contents of caret position
            fieldTextures.resize(network.fields.size());
            fieldZs.resize(network.fields.size(), 0);

            for (int i = 0; i < network.fields.size(); i++) {
                sf::Vector3i fieldSize = network.fields[i].fieldSize;

                // Make sure is in range
                fieldZs[i] = std::min(network.fields[i].fieldSize.z - 1, std::max(0, fieldZs[i]));

                int empty = (fieldSize.x * fieldSize.y * fieldSize.z) == 0;

                sf::Image wImg;

                if (empty)
                    wImg.create(1, 1);
                else {
                    // If can use RGB for pre-encoder
                    if (fieldSize.z == 3 && caret.layer < network.numEncs) {
                        wImg.create(fieldSize.x, fieldSize.y, sf::Color::Black);

                        for (int x = 0; x < wImg.getSize().x; x++)
                            for (int y = 0; y < wImg.getSize().y; y++) {
                                int indexR = 0 + 3 * (y + fieldSize.y * x);
                                int indexG = 1 + 3 * (y + fieldSize.y * x);
                                int indexB = 2 + 3 * (y + fieldSize.y * x);

                                field_type r = network.fields[i].field[indexR];
                                field_type g = network.fields[i].field[indexG];
                                field_type b = network.fields[i].field[indexB];

                                wImg.setPixel(x, y, sf::Color(r, g, b));
                            }
                    }
                    else { // Seperate channels
                        wImg.create(fieldSize.x, fieldSize.y, sf::Color::Black);

                        for (int x = 0; x < wImg.getSize().x; x++)
                            for (int y = 0; y < wImg.getSize().y; y++) {
                                int index = fieldZs[i] + y * network.fields[i].fieldSize.z + x * network.fields[i].fieldSize.y * network.fields[i].fieldSize.z;

                                field_type value = network.fields[i].field[index];

                                wImg.setPixel(x, y, sf::Color(value, value, value));
                            }
                    }
                }

                fieldTextures[i].loadFromImage(wImg);

                fieldTextures[i].setSmooth(false);

                std::string name = network.fields[i].name.data();

                ImGui::Begin(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);

                bool hovering;
                int hoverX = -1;
                int hoverY = -1;

                ImGui::ImageHover(fieldTextures[i], hovering, hoverX, hoverY, ImVec2(8.0f * wImg.getSize().x, 8.0f * wImg.getSize().y));

                if (hovering) {
                    // Select field Z
                    fieldZs[i] = std::min(network.fields[i].fieldSize.z - 1, std::max(0, fieldZs[i] + mouseWheelDelta));

                    ImGui::BeginTooltip();

                    if ((network.fields[i].fieldSize.z == 3 || network.fields[i].fieldSize.z == 6) && caret.layer < network.numEncs)
                        ImGui::SetTooltip("RGB");
                    else
                        ImGui::SetTooltip(("Z: " + std::to_string(fieldZs[i])).c_str());

                    ImGui::EndTooltip();
                }

                ImGui::End();
            }
        }

        window.setView(sf::View(sf::FloatRect(0.0f, 0.0f, window.getSize().x, window.getSize().y)));

        window.clear(sf::Color(50, 50, 50));

        //sf::Sprite logoSprite;
        //logoSprite.setTexture(logoTex);
        //logoSprite.setOrigin(sf::Vector2f(logoTex.getSize().x * 0.5f, logoTex.getSize().y * 0.5f));
        //logoSprite.setPosition(sf::Vector2f(window.getSize().x * 0.5f, window.getSize().y * 0.5f));
        //logoSprite.setColor(sf::Color(255, 255, 255, 80));

        //window.draw(logoSprite);

        window.resetGLStates();

        ImGui::Render();
        window.display();
    }

    std:: cout << "Stopping receiving..." << std::endl;

    if (receiveThread != nullptr) {
        socket.disconnect();

        endReceiving();
    }

    std:: cout << "Stopping connections..." << std::endl;

    if (connectThread != nullptr)
        connectThread->join();

    ImGui::SFML::Shutdown();

    std:: cout << "Bye!" << std::endl;
}

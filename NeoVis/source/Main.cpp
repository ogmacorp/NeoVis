// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017-2019 Ogma Intelligent Systems Corp. All rights reserved.
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
#include <fstream>
#include <iostream>

const int maxStr = 128;

enum ConnectionStatus {
    _disconnected, _connected, _connecting
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
    connectionStatus = _connecting;

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

        connectionStatus = _disconnected;

        return;
    }

    std::cout << "Connecting to address: \"" << shortAddress << "\" on port \"" << port << "\" ..." << std::endl;

    sf::Socket::Status status = socket->connect(shortAddress, port, sf::seconds(5.0f));
    
    if (status == sf::Socket::Status::Done) {
        connectionStatus = _connected;

        std::cout << "Connection established!" << std::endl;

        if (receiveThread != nullptr)
            endReceiving();

        // Start receiving
        receiveThread.reset(new std::thread(receiveThreadFunc, socket));

        return;
    }
    else {
        connectionStatus = _disconnected;

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
    sf::Uint16 _layer;
    sf::Vector3i _pos;

    Caret()
    : _layer(0),
    _pos(-1, -1, -1)
    {}
};

struct SDR {
    sf::Uint16 _width, _height, _columnSize;
    std::vector<sf::Uint16> _indices;
};

struct Field {
    std::array<char, 32> _name;
    sf::Vector3i _fieldSize;
    std::vector<float> _field;
};

struct Network {
    sf::Uint16 _numLayers;
    std::vector<SDR> _sdrs;

    std::vector<Field> _fields;

    Network()
    : _numLayers(0)
    {}
};

Network bufferedNetwork;
Network network;
Caret caret;

bool recv(sf::TcpSocket* socket, char* data, int size) {
    int numReceived = 0;

    while (numReceived < size) {
        size_t received;

        sf::Socket::Status s = socket->receive(&data[numReceived], size - numReceived, received);

        if (s != sf::Socket::Status::Done) {
            connectionStatus = _disconnected;
            return false;
        }
            
        numReceived += received;
    }

    return true;
}

void receiveThreadFunc(sf::TcpSocket* socket) {
    while (!stopReceiving) {
        if (recv(socket, reinterpret_cast<char*>(&bufferedNetwork._numLayers), sizeof(sf::Uint16))) {
            bufferedNetwork._sdrs.resize(bufferedNetwork._numLayers);

            for (int l = 0; l < bufferedNetwork._numLayers; l++) {
                recv(socket, reinterpret_cast<char*>(&bufferedNetwork._sdrs[l]._width), sizeof(sf::Uint16));
                recv(socket, reinterpret_cast<char*>(&bufferedNetwork._sdrs[l]._height), sizeof(sf::Uint16));
                recv(socket, reinterpret_cast<char*>(&bufferedNetwork._sdrs[l]._columnSize), sizeof(sf::Uint16));

                bufferedNetwork._sdrs[l]._indices.resize(bufferedNetwork._sdrs[l]._width * bufferedNetwork._sdrs[l]._height);
                
                recv(socket, reinterpret_cast<char*>(bufferedNetwork._sdrs[l]._indices.data()), bufferedNetwork._sdrs[l]._indices.size() * sizeof(sf::Uint16));
            }

            // Number of fields
            sf::Uint16 numFields;

            recv(socket, reinterpret_cast<char*>(&numFields), sizeof(sf::Uint16));
            
            bufferedNetwork._fields.resize(numFields);

            for (int f = 0; f < numFields; f++) {
                recv(socket, reinterpret_cast<char*>(&bufferedNetwork._fields[f]._name), sizeof(bufferedNetwork._fields[f]._name));
                recv(socket, reinterpret_cast<char*>(&bufferedNetwork._fields[f]._fieldSize), sizeof(sf::Vector3i));
                int totalSize = bufferedNetwork._fields[f]._fieldSize.x * bufferedNetwork._fields[f]._fieldSize.y * bufferedNetwork._fields[f]._fieldSize.z;

                bufferedNetwork._fields[f]._field.resize(totalSize);
                recv(socket, reinterpret_cast<char*>(bufferedNetwork._fields[f]._field.data()), bufferedNetwork._fields[f]._field.size() * sizeof(float));
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

    sf::Texture logoTex;
    logoTex.loadFromFile("resources/logo.png");
    logoTex.setSmooth(true);

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
                case _connected:
                    statusStr = "Connected!";
                    break;
                case _connecting:
                    statusStr = "Connecting...";
                    break;
                case _disconnected:
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

        if (connectionStatus == _disconnected) {
            layerCSDRVis.clear();
            fieldTextures.clear();
        }
        else if (connectionStatus == _connected) {
            // Send Caret
            socket.send(reinterpret_cast<char*>(&caret), sizeof(Caret));

            network = bufferedNetwork;

            // Init
            if (layerCSDRVis.empty()) {
                layerCSDRVis.resize(network._numLayers);

                for (int l = 0; l < network._numLayers; l++)
                    layerCSDRVis[l].init(network._sdrs[l]._width, network._sdrs[l]._height, network._sdrs[l]._columnSize);
            }

            // Visualize content
            for (int l = 0; l < network._numLayers; l++) {
                SDR &sdr = network._sdrs[l];

                for (int i = 0; i < sdr._indices.size(); i++)
                    layerCSDRVis[l][i] = sdr._indices[i];

                layerCSDRVis[l].draw();

                ImGui::Begin(("Layer " + std::to_string(l)).c_str());

                bool hovering;
                int hoverX = -1;
                int hoverY = -1;

                ImGui::ImageHover(layerCSDRVis[l].getTexture(), hovering, hoverX, hoverY, ImVec2(layerCSDRVis[l].getTexture().getSize().x, layerCSDRVis[l].getTexture().getSize().y));

                if (hovering) {
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
                        // Divide by size
                        layerCSDRVis[l]._highlightX = static_cast<int>(hoverX / layerCSDRVis[l]._nodeSpaceSize);
                        layerCSDRVis[l]._highlightY = layerCSDRVis[l].getSizeInNodes().y - static_cast<int>(hoverY / layerCSDRVis[l]._nodeSpaceSize + 1.0f);

                        caret._pos = layerCSDRVis[l].getHighlightedCSDRPos();
                        caret._layer = l;
                    }
                }
                else {
                    layerCSDRVis[l]._highlightX = -1;
                    layerCSDRVis[l]._highlightY = -1;
                }
    
                ImGui::End();
            }

            // Show contents of caret position
            fieldTextures.resize(network._fields.size());
            fieldZs.resize(network._fields.size(), 0);

            for (int i = 0; i < network._fields.size(); i++) {
                sf::Vector3i fieldSize = network._fields[i]._fieldSize;

                // Make sure is in range
                fieldZs[i] = std::min(network._fields[i]._fieldSize.z - 1, std::max(0, fieldZs[i]));

                sf::Image wImg;

                wImg.create(fieldSize.x, fieldSize.y, sf::Color::Black);

                for (int x = 0; x < wImg.getSize().x; x++)
                    for (int y = 0; y < wImg.getSize().y; y++) {
                        int index = fieldZs[i] + y * network._fields[i]._fieldSize.z + x * network._fields[i]._fieldSize.y * network._fields[i]._fieldSize.z;

                        float value = network._fields[i]._field[index];

                        value = std::exp(value); // Transform for ESE encoder

                        sf::Uint8 g = std::min(1.0f, std::max(0.0f, value)) * 255;

                        wImg.setPixel(x, y, sf::Color(g, g, g));
                    }

                fieldTextures[i].loadFromImage(wImg);

                fieldTextures[i].setSmooth(false);

                std::string name = network._fields[i]._name.data();

                ImGui::Begin(name.c_str());

                bool hovering;
                int hoverX = -1;
                int hoverY = -1;

                ImGui::ImageHover(fieldTextures[i], hovering, hoverX, hoverY, ImVec2(8.0f * wImg.getSize().x, 8.0f * wImg.getSize().y));

                if (hovering) {
                    // Select field Z
                    fieldZs[i] = std::min(network._fields[i]._fieldSize.z - 1, std::max(0, fieldZs[i] + mouseWheelDelta));

                    ImGui::BeginTooltip();
                    ImGui::SetTooltip(("Z: " + std::to_string(fieldZs[i])).c_str());
                    ImGui::EndTooltip();
                }

                ImGui::End();
            }
        }

        window.setView(sf::View(sf::FloatRect(0.0f, 0.0f, window.getSize().x, window.getSize().y)));

        window.clear(sf::Color(50, 50, 50));

        sf::Sprite logoSprite;
        logoSprite.setTexture(logoTex);
        logoSprite.setOrigin(sf::Vector2f(logoTex.getSize().x * 0.5f, logoTex.getSize().y * 0.5f));
        logoSprite.setPosition(sf::Vector2f(window.getSize().x * 0.5f, window.getSize().y * 0.5f));
        logoSprite.setColor(sf::Color(255, 255, 255, 80));
        logoSprite.setScale(sf::Vector2f(0.4f, 0.4f));

        window.draw(logoSprite);

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

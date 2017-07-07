// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of NeoVis is licensed to you under the terms described
//  in the NEOVIS_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#include "imgui/imgui.h"
#include "imgui/imgui-SFML.h"
#include "imgui_extra.h"

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

struct SDR {
    sf::Uint16 _chunkSize;
    sf::Uint16 _width, _height;
    std::vector<sf::Uint16> _chunkIndices;
};

sf::Packet &operator >> (sf::Packet &packet, SDR &sdr) {
    packet >> sdr._chunkSize >> sdr._width >> sdr._height;

    sdr._chunkIndices.resize(sdr._width * sdr._height);

    for (int i = 0; i < sdr._chunkIndices.size(); i++)
        packet >> sdr._chunkIndices[i];

    return packet;
}

struct WeightSet {
    std::string _name;
    sf::Uint16 _radius;
    std::vector<float> _weights;
};

sf::Packet &operator >> (sf::Packet &packet, WeightSet &weightSet) {
    packet >> weightSet._name >> weightSet._radius;

    sf::Uint16 diam = weightSet._radius * 2 + 1;

    weightSet._weights.resize(diam * diam);

    for (int i = 0; i < weightSet._weights.size(); i++)
        packet >> weightSet._weights[i];

    return packet;
}

struct Network {
    sf::Uint16 _numLayers;
    sf::Uint16 _numWeightSets;
    std::vector<SDR> _sdrs;
    std::vector<WeightSet> _weightSets;
};

sf::Packet &operator >> (sf::Packet &packet, Network &network) {
    packet >> network._numLayers >> network._numWeightSets;

    network._sdrs.resize(network._numLayers);

    for (int i = 0; i < network._sdrs.size(); i++)
        packet >> network._sdrs[i];

    network._weightSets.resize(network._numWeightSets);

    for (int i = 0; i < network._weightSets.size(); i++)
        packet >> network._weightSets[i];

    return packet;
}

struct Caret {
    sf::Uint16 _layer;
    sf::Uint16 _bitIndex;
};

Caret caret;

sf::Packet &operator << (sf::Packet &packet, const Caret &caret) {
    return packet << caret._layer << caret._bitIndex;
}

Network bufferedNetwork;
Network network;

std::mutex receiveMutex;

void receiveThreadFunc(sf::TcpSocket* socket) {
    while (!stopReceiving) {
        sf::Packet packet;

        socket->receive(packet);

        {
            std::lock_guard<std::mutex> lock(receiveMutex);

            packet >> bufferedNetwork;
        }

        sf::sleep(sf::seconds(0.001f));
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

    std::vector<sf::Texture> layerTextures;
    std::vector<sf::Texture> weightSetTextures;

    sf::TcpSocket socket;

    // ---------------------------- Loop ----------------------------

    sf::Clock deltaClock;

    while (window.isOpen()) {
        sf::Event event;

        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed)
                window.close();
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

        // Retrieve network
        if (connectionStatus == _connected) {
            // Send Caret
            sf::Packet packet;

            packet << caret;

            socket.send(packet);

            {
                std::lock_guard<std::mutex> lock(receiveMutex);

                network = bufferedNetwork;
            }

            // Visualize content
            layerTextures.resize(network._numLayers);

            for (int l = 0; l < network._numLayers; l++) {
                SDR &sdr = network._sdrs[l];

                sf::Image sdrImg;

                sdrImg.create(sdr._width * sdr._chunkSize, sdr._height * sdr._chunkSize, sf::Color::Black);

                layerTextures[l].loadFromImage(sdrImg);

                ImGui::Begin(("Layer " + std::to_string(l)).c_str());

                bool hovering;
                int hoverIndex;

                ImGui::ImageHover(layerTextures[l], hovering, hoverIndex, ImVec2(4.0f * sdrImg.getSize().x, 4.0f * sdrImg.getSize().y));

                if (hovering && sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
                    caret._bitIndex = hoverIndex;
                    caret._layer = l;
                }
    
                int chunksInX = sdr._width;
                int chunksInY = sdr._height;
                
                for (int cx = 0; cx < chunksInX; cx++)
                    for (int cy = 0; cy < chunksInY; cy++) {
                        int index = cx + cy * chunksInX;

                        int dx = sdr._chunkIndices[index] % sdr._chunkSize;
                        int dy = sdr._chunkIndices[index] / sdr._chunkSize;

                        int x = cx * sdr._chunkSize + dx;
                        int y = cy * sdr._chunkSize + dy;

                        sdrImg.setPixel(x, y, sf::Color::White);
                    }

                if (hovering)
                    sdrImg.setPixel(caret._bitIndex % sdrImg.getSize().x, caret._bitIndex / sdrImg.getSize().x, sf::Color::Green);

                layerTextures[l].loadFromImage(sdrImg);

                layerTextures[l].setSmooth(false);  

                ImGui::End();
            }

            // Show contents of Caret position
            weightSetTextures.resize(network._numWeightSets);

            for (int i = 0; i < network._numWeightSets; i++) {
                int diam = network._weightSets[i]._radius * 2 + 1;

                sf::Image wImg;

                wImg.create(diam, diam, sf::Color::Black);

                weightSetTextures[i].loadFromImage(wImg);

                ImGui::Begin(network._weightSets[i]._name.c_str());

                bool hovering;
                int hoverIndex;

                ImGui::ImageHover(weightSetTextures[i], hovering, hoverIndex, ImVec2(4.0f * wImg.getSize().x, 4.0f * wImg.getSize().y));

                for (int x = 0; x < wImg.getSize().x; x++)
                    for (int y = 0; y < wImg.getSize().y; y++) {
                        int index = x + y * wImg.getSize().x;

                        float weight = network._weightSets[i]._weights[index];

                        sf::Uint8 grey = std::min(1.0f, std::max(0.0f, weight)) * 255;

                        wImg.setPixel(x, y, sf::Color(grey, grey, grey));
                    }

                if (hovering) {
                    wImg.setPixel(hoverIndex % wImg.getSize().x, hoverIndex / wImg.getSize().x, sf::Color::Green);

                    /*ImGui::BeginTooltip();

                    ImGui::SetTooltip("Eerwer");

                    ImGui::EndTooltip();*/
                }

                weightSetTextures[i].loadFromImage(wImg);

                weightSetTextures[i].setSmooth(false);

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

    if (receiveThread != nullptr) {
        socket.disconnect();

        endReceiving();
    }

    if (connectThread != nullptr)
        connectThread->join();

    ImGui::SFML::Shutdown();
}

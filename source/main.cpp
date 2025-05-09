// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017-2024 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of NeoVis is licensed to you under the terms described
//  in the NEOVIS_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui-SFML.h"
#include "imgui_extra.h"
#include "CSDR_Vis.h"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <thread>
#include <mutex>
#include <array>
#include <fstream>
#include <iostream>

const int max_str = 128;

typedef unsigned char field_type;

enum Connection_Status {
    disconnected, connected, connecting
};

Connection_Status connection_status;
std::string address_str;
std::string port_str;

std::unique_ptr<std::thread> connect_thread;
std::unique_ptr<std::thread> receive_thread;

bool stop_receiving = false;

void receive_thread_func(
    sf::TcpSocket* socket
);

void enc_receiving() {
    if (receive_thread != nullptr) {
        stop_receiving = true;

        receive_thread->join();

        stop_receiving = false;
    }
}

void connect_thread_func(
    sf::TcpSocket* socket
) {
    connection_status = connecting;

    std::string shortAddress = address_str;

    int address_length;

    for (address_length = 0; address_length < shortAddress.size(); address_length++) {
        if (shortAddress[address_length] == ' ' || shortAddress[address_length] == '\0')
            break;
    }

    shortAddress.resize(address_length);

    int port;

    try {
        port = std::stoi(port_str);
    }
    catch (std::exception e) {
        std::cout << "Invalid port!";

        connection_status = disconnected;

        return;
    }

    std::cout << "Connecting to address: \"" << shortAddress << "\" on port \"" << port << "\" ..." << std::endl;

    std::optional<sf::IpAddress> addr = sf::IpAddress::resolve(shortAddress);

    if (!addr.has_value()) {
        std::cout << "Could not resolve IP address!" << std::endl;

        return;
    }

    sf::Socket::Status status = socket->connect(addr.value(), port, sf::seconds(5.0f));
    
    if (status == sf::Socket::Status::Done) {
        connection_status = connected;

        std::cout << "Connection established!" << std::endl;

        if (receive_thread != nullptr)
            enc_receiving();

        // Start receiving
        receive_thread.reset(new std::thread(receive_thread_func, socket));

        return;
    }
    else {
        connection_status = disconnected;

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
    std::uint16_t layer;
    sf::Vector3i pos;

    Caret()
    : layer(0),
    pos(-1, -1, -1)
    {}
};

struct CSDR {
    std::uint16_t width, height, column_size;
    std::vector<std::uint16_t> indices;
};

struct Field {
    std::array<char, 64> name;
    std::int32_t field_size_x;
    std::int32_t field_size_y;
    std::int32_t field_size_z;
    std::vector<field_type> field;
};

struct Network {
    std::uint16_t num_layers;
    std::uint16_t num_encs; // Number of layers that are encodeers
    std::vector<CSDR> csdrs;

    std::vector<Field> fields;

    Network()
    :
    num_layers(0)
    {}
};

Network buffered_network;
Network network;
Caret caret;

bool recv(sf::TcpSocket* socket, void* data, int size) {
    int num_received = 0;

    while (num_received < size) {
        size_t received;

        sf::Socket::Status s = socket->receive(&reinterpret_cast<char*>(data)[num_received], size - num_received, received);

        if (s != sf::Socket::Status::Done) {
            connection_status = disconnected;
            return false;
        }
            
        num_received += received;
    }

    return true;
}

void receive_thread_func(sf::TcpSocket* socket) {
    while (!stop_receiving) {
        if (recv(socket, &buffered_network.num_layers, sizeof(std::uint16_t))) {
            recv(socket, &buffered_network.num_encs, sizeof(std::uint16_t));

            buffered_network.csdrs.resize(buffered_network.num_layers);

            for (int l = 0; l < buffered_network.num_layers; l++) {
                recv(socket, &buffered_network.csdrs[l].width, sizeof(std::uint16_t));
                recv(socket, &buffered_network.csdrs[l].height, sizeof(std::uint16_t));
                recv(socket, &buffered_network.csdrs[l].column_size, sizeof(std::uint16_t));

                buffered_network.csdrs[l].indices.resize(buffered_network.csdrs[l].width * buffered_network.csdrs[l].height);
                
                recv(socket, buffered_network.csdrs[l].indices.data(), buffered_network.csdrs[l].indices.size() * sizeof(std::uint16_t));
            }

            // Number of fields
            std::uint16_t num_fields;

            recv(socket, &num_fields, sizeof(std::uint16_t));
            
            buffered_network.fields.resize(num_fields);

            for (int f = 0; f < num_fields; f++) {
                recv(socket, &buffered_network.fields[f].name, sizeof(buffered_network.fields[f].name));
                recv(socket, &buffered_network.fields[f].field_size_x, sizeof(std::int32_t));
                recv(socket, &buffered_network.fields[f].field_size_y, sizeof(std::int32_t));
                recv(socket, &buffered_network.fields[f].field_size_z, sizeof(std::int32_t));

                buffered_network.fields[f].field.resize(buffered_network.fields[f].field_size_x * buffered_network.fields[f].field_size_y * buffered_network.fields[f].field_size_z);
                recv(socket, buffered_network.fields[f].field.data(), buffered_network.fields[f].field.size() * sizeof(field_type));
            }
        }
    }
}

int main() {
    sf::RenderWindow window(sf::VideoMode(sf::Vector2u(1280, 720)), "NeoVis", sf::Style::Default);

    window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    bool initialized = ImGui::SFML::Init(window);

    window.resetGLStates();

    //sf::Texture logoTex;
    //logoTex.loadFromFile("resources/logo.png");
    //logoTex.setSmooth(true);

    // ---------------------------- State ---------------------------

    bool connection_wizard_open = false;

    // Read address and port
    std::ifstream from_config("config.txt");

    if (!from_config.is_open()) {
        std::cout << "Could not open config file!" << std::endl;

        return 0;
    }

    std::getline(from_config, address_str);
    std::getline(from_config, port_str);

    address_str.resize(max_str);
    port_str.resize(max_str);

    std::vector<CSDR_Vis> layer_CSDR_vis;
    std::vector<sf::Texture> field_textures;
    std::vector<int> field_zs;

    sf::TcpSocket socket;

    // ---------------------------- Loop ----------------------------

    sf::Clock delta_clock;

    while (window.isOpen()) {
        int mouse_wheel_delta = 0;

        while (const std::optional event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);

            if (event->is<sf::Event::Closed>()) {
                window.close();

                break;
            }

            if (event->is<sf::Event::MouseWheelScrolled>())
                mouse_wheel_delta = event->getIf<sf::Event::MouseWheelScrolled>()->delta;
        }

        ImGui::SFML::Update(window, delta_clock.restart());

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Connection")) {
                if (ImGui::MenuItem("Connect...")) {
                    connection_wizard_open = true;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (connection_wizard_open) {
            bool open = true;

            if (ImGui::Begin("Connection Wizard", &open)) {
                ImGui::InputText("Address", &address_str[0], address_str.size());
                
                ImGui::NewLine();
                
                ImGui::InputText("Port", &port_str[0], port_str.size());

                ImGui::NewLine();

                std::string status_str;
                
                switch (connection_status) {
                case connected:
                    status_str = "Connected!";
                    break;
                case connecting:
                    status_str = "Connecting...";
                    break;
                case disconnected:
                    status_str = "Disconnected.";
                    break;
                }

                if (ImGui::Button("Connect!")) {
                    if (connect_thread != nullptr)
                        connect_thread->join();

                    connect_thread.reset(new std::thread(connect_thread_func, &socket));
                    
                    // Save configuration
                    std::ofstream to_config("config.txt");

                    to_config << address_str << std::endl;
                    to_config << port_str << std::endl;
                }

                ImGui::LabelText("Status", status_str.c_str());

                ImGui::End();
            }
            
            if (!open)
                connection_wizard_open = false;
        }

        if (connection_status == disconnected) {
            layer_CSDR_vis.clear();
            field_textures.clear();
        }
        else if (connection_status == connected) {
            network = buffered_network;

            // Send Caret
            sf::Socket::Status status = socket.send(&caret, sizeof(Caret));

            // Init
            if (layer_CSDR_vis.empty()) {
                layer_CSDR_vis.resize(network.num_layers);

                for (int l = 0; l < network.num_layers; l++)
                    layer_CSDR_vis[l].init(network.csdrs[l].width, network.csdrs[l].height, network.csdrs[l].column_size);
            }

            // Visualize content
            for (int l = 0; l < network.num_layers; l++) {
                CSDR &csdr = network.csdrs[l];

                for (int i = 0; i < csdr.indices.size(); i++)
                    layer_CSDR_vis[l][i] = csdr.indices[i];

                layer_CSDR_vis[l].draw();

                if (l < network.num_encs)
                    ImGui::Begin(("Pre-encoder " + std::to_string(l)).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                else
                    ImGui::Begin(("Layer " + std::to_string(l - network.num_encs)).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);

                bool hovering;
                int hover_x = -1;
                int hover_y = -1;

                ImGui::ImageHover(layer_CSDR_vis[l].get_texture(), hovering, hover_x, hover_y, ImVec2(layer_CSDR_vis[l].get_texture().getSize().x, layer_CSDR_vis[l].get_texture().getSize().y));

                if (hovering) {
                    if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Right)) {
                        // Divide by size
                        layer_CSDR_vis[l].highlight_x = static_cast<int>(hover_x / layer_CSDR_vis[l].node_space_size);
                        layer_CSDR_vis[l].highlight_y = layer_CSDR_vis[l].get_size_in_nodes().y - static_cast<int>(hover_y / layer_CSDR_vis[l].node_space_size + 1.0f);

                        caret.pos = layer_CSDR_vis[l].get_highlighted_CSDR_pos();
                        caret.layer = l;
                    }
                }
                else {
                    layer_CSDR_vis[l].highlight_x = -1;
                    layer_CSDR_vis[l].highlight_y = -1;
                }
    
                ImGui::End();
            }

            // Show contents of caret position
            field_textures.resize(network.fields.size());
            field_zs.resize(network.fields.size(), 0);

            for (int i = 0; i < network.fields.size(); i++) {
                sf::Vector3i field_size = sf::Vector3i(network.fields[i].field_size_x, network.fields[i].field_size_y, network.fields[i].field_size_z);

                // Make sure is in range
                field_zs[i] = std::min(field_size.z - 1, std::max(0, field_zs[i]));

                int empty = (field_size.x * field_size.y * field_size.z) == 0;

                sf::Image w_img;

                if (empty)
                    w_img = sf::Image(sf::Vector2u(1, 1));
                else {
                    // If can use RGB for pre-encoder
                    if (field_size.z == 3 && caret.layer < network.num_encs) {
                        w_img = sf::Image(sf::Vector2u(field_size.x, field_size.y), sf::Color::Black);

                        for (int x = 0; x < w_img.getSize().x; x++)
                            for (int y = 0; y < w_img.getSize().y; y++) {
                                int indexR = 0 + 3 * (y + field_size.y * x);
                                int indexG = 1 + 3 * (y + field_size.y * x);
                                int indexB = 2 + 3 * (y + field_size.y * x);

                                field_type r = network.fields[i].field[indexR];
                                field_type g = network.fields[i].field[indexG];
                                field_type b = network.fields[i].field[indexB];

                                w_img.setPixel(sf::Vector2u(x, y), sf::Color(r, g, b));
                            }
                    }
                    else { // Seperate channels
                        w_img = sf::Image(sf::Vector2u(field_size.x, field_size.y), sf::Color::Black);

                        for (int x = 0; x < w_img.getSize().x; x++)
                            for (int y = 0; y < w_img.getSize().y; y++) {
                                int index = field_zs[i] + y * field_size.z + x * field_size.y * field_size.z;

                                field_type value = network.fields[i].field[index];

                                w_img.setPixel(sf::Vector2u(x, y), sf::Color(value, value, value));
                            }
                    }
                }

                field_textures[i] = sf::Texture(w_img);

                field_textures[i].setSmooth(false);

                std::string name = network.fields[i].name.data();

                ImGui::Begin(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);

                bool hovering;
                int hover_x = -1;
                int hover_y = -1;

                ImGui::ImageHover(field_textures[i], hovering, hover_x, hover_y, ImVec2(8.0f * w_img.getSize().x, 8.0f * w_img.getSize().y));

                if (hovering) {
                    // Select field Z
                    field_zs[i] = std::min(network.fields[i].field_size_z - 1, std::max(0, field_zs[i] + mouse_wheel_delta));

                    ImGui::BeginTooltip();

                    if ((network.fields[i].field_size_z == 3 || network.fields[i].field_size_z == 6) && caret.layer < network.num_encs)
                        ImGui::SetTooltip("RGB");
                    else
                        ImGui::SetTooltip(("Z: " + std::to_string(field_zs[i])).c_str());

                    ImGui::EndTooltip();
                }

                ImGui::End();
            }
        }

        window.setView(sf::View(sf::FloatRect(sf::Vector2f(0.0f, 0.0f), sf::Vector2f(window.getSize().x, window.getSize().y))));

        window.clear(sf::Color(50, 50, 50));

        //sf::Sprite logoSprite;
        //logoSprite.setTexture(logoTex);
        //logoSprite.setOrigin(sf::Vector2f(logoTex.getSize().x * 0.5f, logoTex.getSize().y * 0.5f));
        //logoSprite.setPosition(sf::Vector2f(window.getSize().x * 0.5f, window.getSize().y * 0.5f));
        //logoSprite.setColor(sf::Color(255, 255, 255, 80));

        //window.draw(logoSprite);

        window.resetGLStates();

        ImGui::SFML::Render(window);
        window.display();
    }

    std:: cout << "Stopping receiving..." << std::endl;

    if (receive_thread != nullptr) {
        socket.disconnect();

        enc_receiving();
    }

    std:: cout << "Stopping connections..." << std::endl;

    if (connect_thread != nullptr)
        connect_thread->join();

    ImGui::SFML::Shutdown();

    std:: cout << "Bye!" << std::endl;
}

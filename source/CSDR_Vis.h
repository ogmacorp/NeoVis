// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017-2024 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of NeoVis is licensed to you under the terms described
//  in the NEOVIS_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <memory>

class CSDR_Vis {
private:
    std::shared_ptr<sf::RenderTexture> rt;

    std::vector<int> columns;

    int width, height, column_size;
    int root_column_size;

    void draw_column(const sf::Vector2f &position, int index, bool isOdd, int cx, int cy);

    sf::Vector3i highlighted_CSDR_pos;

public:
    float edge_radius;
    float node_space_size;
    float node_outer_ratio;
    float node_inner_ratio;
    
    int edge_segments;
    int node_outer_segments;
    int node_inner_segments;

    sf::Color background_color0;
    sf::Color background_color1;
    sf::Color node_outer_color;
    sf::Color node_inner_color;

    sf::Color node_inner_color_highlight;

    int highlight_x, highlight_y;

    CSDR_Vis()
        : edge_radius(2.0f), node_space_size(8.0f), node_outer_ratio(0.85f), node_inner_ratio(0.75f),
        edge_segments(16), node_outer_segments(16), node_inner_segments(16),
        background_color0(98, 98, 98), background_color1(168, 168, 168), node_outer_color(64, 64, 64), node_inner_color(255, 0, 0),
        node_inner_color_highlight(0, 255, 0),
        highlight_x(-1), highlight_y(-1),
        highlighted_CSDR_pos(-1, -1, -1)
    {}

    void init(int width, int height, int column_size);

    int &operator[](int index) {
        return columns[index];
    }

    int &at(int x, int y) {
        return columns[y + x * height];
    }

    void draw();

    sf::Vector2i get_size_in_nodes() const {
        return sf::Vector2i(width * root_column_size, height * root_column_size);
    }

    const sf::Texture &get_texture() const {
        return rt->getTexture();
    }

    const sf::Vector3i &get_highlighted_CSDR_pos() const {
        return highlighted_CSDR_pos;
    }
};

// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017-2024 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of NeoVis is licensed to you under the terms described
//  in the NEOVIS_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#include "CSDR_Vis.h"
#include <cmath>

void CSDR_Vis::init(int width, int height, int column_size) {
    this->width = width;
    this->height = height;
    this->column_size = column_size;

    root_column_size = std::ceil(std::sqrt(static_cast<float>(column_size)));

    columns.resize(width * height, 0);

    float r_size = node_space_size * root_column_size;

    rt = std::make_unique<sf::RenderTexture>();

    rt->create(static_cast<int>(std::ceil(width * r_size)), static_cast<int>(std::ceil(height * r_size)));
}

void CSDR_Vis::draw_column(const sf::Vector2f &position, int index, bool isOdd, int cx, int cy) {
    float r_size = node_space_size * root_column_size;

    sf::Color backgroundColor = isOdd ? background_color1 : background_color0;

    sf::RectangleShape rs_horizontal;
    rs_horizontal.setPosition(position + sf::Vector2f(0.0f, edge_radius));
    rs_horizontal.setSize(sf::Vector2f(r_size, r_size - edge_radius * 2.0f));
    rs_horizontal.setFillColor(backgroundColor);

    rt->draw(rs_horizontal);

    sf::RectangleShape rs_vertical;
    rs_vertical.setPosition(position + sf::Vector2f(edge_radius, 0.0f));
    rs_vertical.setSize(sf::Vector2f(r_size - edge_radius * 2.0f, r_size));
    rs_vertical.setFillColor(backgroundColor);

    rt->draw(rs_vertical);

    // Corners
    sf::CircleShape corner;
    corner.setRadius(edge_radius);
    corner.setPointCount(edge_segments);
    corner.setFillColor(backgroundColor);
    corner.setOrigin(sf::Vector2f(edge_radius, edge_radius));

    corner.setPosition(position + sf::Vector2f(edge_radius, edge_radius));
    rt->draw(corner);

    corner.setPosition(position + sf::Vector2f(r_size - edge_radius, edge_radius));
    rt->draw(corner);

    corner.setPosition(position + sf::Vector2f(r_size - edge_radius, r_size - edge_radius));
    rt->draw(corner);

    corner.setPosition(position + sf::Vector2f(edge_radius, r_size - edge_radius));
    rt->draw(corner);

    // Nodes
    sf::CircleShape outer;
    outer.setRadius(node_space_size * node_outer_ratio * 0.5f);
    outer.setPointCount(node_outer_segments);
    outer.setFillColor(node_outer_color);
    outer.setOrigin(sf::Vector2f(outer.getRadius(), outer.getRadius()));

    sf::CircleShape inner;
    inner.setRadius(node_space_size * node_outer_ratio * node_inner_ratio * 0.5f);
    inner.setPointCount(node_inner_segments);
    //inner.setFillColor(node_inner_color);
    inner.setOrigin(sf::Vector2f(inner.getRadius(), inner.getRadius()));

    for (int x = 0; x < root_column_size; x++)
        for (int y = 0; y < root_column_size; y++) {
            int subIndex = x + y * root_column_size;

            if (subIndex < column_size) {
                outer.setPosition(position + sf::Vector2f(x * node_space_size + edge_radius * 2.0f, y * node_space_size + edge_radius * 2.0f));

                rt->draw(outer);

                inner.setPosition(position + sf::Vector2f(x * node_space_size + edge_radius * 2.0f, y * node_space_size + edge_radius * 2.0f));

                int tx = cx * root_column_size + x;
                int ty = cy * root_column_size + y;

                bool highlight = (tx == highlight_x && ty == highlight_y);

                if (highlight) {
                    highlighted_CSDR_pos.x = cx;
                    highlighted_CSDR_pos.y = cy;
                    highlighted_CSDR_pos.z = subIndex;
                }

                sf::Color sub_node_inner_color = highlight ? node_inner_color_highlight : node_inner_color;

                inner.setFillColor(sf::Color(sub_node_inner_color.r, sub_node_inner_color.g, sub_node_inner_color.b, subIndex == index || highlight ? 255 : 0));

                rt->draw(inner);
            }
        }
}

void CSDR_Vis::draw() {
    rt->clear(sf::Color::Transparent);

    float r_size = node_space_size * root_column_size;

    for (int x = 0; x < width; x++)
        for (int y = 0; y < height; y++)
            draw_column(sf::Vector2f(x * r_size, y * r_size), at(x, y), x % 2 != y % 2, x, y);

    rt->display();
}

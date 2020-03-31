// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017-2020 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of NeoVis is licensed to you under the terms described
//  in the NEOVIS_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <memory>

class CSDRVis {
private:
    std::shared_ptr<sf::RenderTexture> rt;

    std::vector<int> columns;

    int width, height, columnSize;
    int rootColumnSize;

    void drawColumn(const sf::Vector2f &position, int index, bool isOdd, int cx, int cy);

    sf::Vector3i highlightedCSDRPos;

public:
    float edgeRadius;
    float nodeSpaceSize;
    float nodeOuterRatio;
    float nodeInnerRatio;
    
    int edgeSegments;
    int nodeOuterSegments;
    int nodeInnerSegments;

    sf::Color backgroundColor0;
    sf::Color backgroundColor1;
    sf::Color nodeOuterColor;
    sf::Color nodeInnerColor;

    sf::Color nodeInnerColorHighlight;

    int highlightX, highlightY;

    CSDRVis()
        : edgeRadius(4.0f), nodeSpaceSize(16.0f), nodeOuterRatio(0.85f), nodeInnerRatio(0.75f),
        edgeSegments(16), nodeOuterSegments(16), nodeInnerSegments(16),
        backgroundColor0(98, 98, 98), backgroundColor1(168, 168, 168), nodeOuterColor(64, 64, 64), nodeInnerColor(255, 0, 0),
        nodeInnerColorHighlight(0, 255, 0),
        highlightX(-1), highlightY(-1),
        highlightedCSDRPos(-1, -1, -1)
    {}

    void init(int width, int height, int columnSize);

    int &operator[](int index) {
        return columns[index];
    }

    int &at(int x, int y) {
        return columns[y + x * height];
    }

    void draw();

    sf::Vector2i getSizeInNodes() const {
        return sf::Vector2i(width * rootColumnSize, height * rootColumnSize);
    }

    const sf::Texture &getTexture() const {
        return rt->getTexture();
    }

    const sf::Vector3i &getHighlightedCSDRPos() const {
        return highlightedCSDRPos;
    }
};
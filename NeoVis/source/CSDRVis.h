// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017-2019 Ogma Intelligent Systems Corp. All rights reserved.
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
    std::shared_ptr<sf::RenderTexture> _rt;

    std::vector<int> _columns;

    int _width, _height, _columnSize;
    int _rootColumnSize;

    void drawColumn(const sf::Vector2f &position, int index, bool isOdd);

public:
    float _edgeRadius;
    float _nodeSpaceSize;
    float _nodeOuterRatio;
    float _nodeInnerRatio;
    
    int _edgeSegments;
    int _nodeOuterSegments;
    int _nodeInnerSegments;

    sf::Color _backgroundColor0;
    sf::Color _backgroundColor1;
    sf::Color _nodeOuterColor;
    sf::Color _nodeInnerColor;

    CSDRVis()
        : _edgeRadius(4.0f), _nodeSpaceSize(16.0f), _nodeOuterRatio(0.85f), _nodeInnerRatio(0.75f),
        _edgeSegments(16), _nodeOuterSegments(16), _nodeInnerSegments(16),
        _backgroundColor0(98, 98, 98), _backgroundColor1(168, 168, 168), _nodeOuterColor(64, 64, 64), _nodeInnerColor(255, 0, 0)
    {}

    void init(int width, int height, int columnSize);

    int &operator[](int index) {
        return _columns[index];
    }

    int &at(int x, int y) {
        return _columns[x + y * _width];
    }

    void draw();

    const sf::Texture &getTexture() const {
        return _rt->getTexture();
    }
};
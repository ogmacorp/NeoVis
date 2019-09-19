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

    void drawColumn(const sf::Vector2f &position, int index, bool isOdd, int cx, int cy);

    sf::Vector3i _highlightedCSDRPos;

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

    sf::Color _nodeInnerColorHighlight;

    int _highlightX, _highlightY;

    CSDRVis()
        : _edgeRadius(4.0f), _nodeSpaceSize(16.0f), _nodeOuterRatio(0.85f), _nodeInnerRatio(0.75f),
        _edgeSegments(16), _nodeOuterSegments(16), _nodeInnerSegments(16),
        _backgroundColor0(98, 98, 98), _backgroundColor1(168, 168, 168), _nodeOuterColor(64, 64, 64), _nodeInnerColor(255, 0, 0),
        _nodeInnerColorHighlight(0, 255, 0),
        _highlightX(-1), _highlightY(-1),
        _highlightedCSDRPos(-1, -1, -1)
    {}

    void init(int width, int height, int columnSize);

    int &operator[](int index) {
        return _columns[index];
    }

    int &at(int x, int y) {
        return _columns[x + y * _width];
    }

    void draw();

    sf::Vector2i getSizeInNodes() const {
        return sf::Vector2i(_width * _rootColumnSize, _height * _rootColumnSize);
    }

    const sf::Texture &getTexture() const {
        return _rt->getTexture();
    }

    const sf::Vector3i &getHighlightedCSDRPos() const {
        return _highlightedCSDRPos;
    }
};
// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017-2019 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of NeoVis is licensed to you under the terms described
//  in the NEOVIS_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#include "CSDRVis.h"
#include <cmath>
#include <iostream>
void CSDRVis::init(int width, int height, int columnSize) {
	_width = width;
	_height = height;
    _columnSize = columnSize;
    _rootColumnSize = std::ceil(std::sqrt(static_cast<float>(_columnSize)));

	_columns.clear();
	_columns.assign(_width * _height, 0);

    float rSize = _nodeSpaceSize * _rootColumnSize;

    _rt.reset(new sf::RenderTexture());

    _rt->create(static_cast<int>(std::ceil(width * rSize)), static_cast<int>(std::ceil(height * rSize)));
}

void CSDRVis::drawColumn(const sf::Vector2f &position, int index, bool isOdd, int cx, int cy) {
    float rSize = _nodeSpaceSize * _rootColumnSize;

    sf::Color backgroundColor = isOdd ? _backgroundColor1 : _backgroundColor0;

	sf::RectangleShape rsHorizontal;
	rsHorizontal.setPosition(position + sf::Vector2f(0.0f, _edgeRadius));
	rsHorizontal.setSize(sf::Vector2f(rSize, rSize - _edgeRadius * 2.0f));
	rsHorizontal.setFillColor(backgroundColor);
	
	_rt->draw(rsHorizontal);

	sf::RectangleShape rsVertical;
	rsVertical.setPosition(position + sf::Vector2f(_edgeRadius, 0.0f));
	rsVertical.setSize(sf::Vector2f(rSize - _edgeRadius * 2.0f, rSize));
	rsVertical.setFillColor(backgroundColor);

	_rt->draw(rsVertical);

	// Corners
	sf::CircleShape corner;
	corner.setRadius(_edgeRadius);
	corner.setPointCount(_edgeSegments);
	corner.setFillColor(backgroundColor);
	corner.setOrigin(sf::Vector2f(_edgeRadius, _edgeRadius));

	corner.setPosition(position + sf::Vector2f(_edgeRadius, _edgeRadius));
	_rt->draw(corner);

	corner.setPosition(position + sf::Vector2f(rSize - _edgeRadius, _edgeRadius));
	_rt->draw(corner);

	corner.setPosition(position + sf::Vector2f(rSize - _edgeRadius, rSize - _edgeRadius));
	_rt->draw(corner);

	corner.setPosition(position + sf::Vector2f(_edgeRadius, rSize - _edgeRadius));
	_rt->draw(corner);

	// Nodes
	sf::CircleShape outer;
	outer.setRadius(_nodeSpaceSize * _nodeOuterRatio * 0.5f);
	outer.setPointCount(_nodeOuterSegments);
	outer.setFillColor(_nodeOuterColor);
	outer.setOrigin(sf::Vector2f(outer.getRadius(), outer.getRadius()));

	sf::CircleShape inner;
	inner.setRadius(_nodeSpaceSize * _nodeOuterRatio * _nodeInnerRatio * 0.5f);
	inner.setPointCount(_nodeInnerSegments);
	//inner.setFillColor(_nodeInnerColor);
	inner.setOrigin(sf::Vector2f(inner.getRadius(), inner.getRadius()));

	for (int x = 0; x < _rootColumnSize; x++)
		for (int y = 0; y < _rootColumnSize; y++) {
            int subIndex = x + y * _rootColumnSize;

            if (subIndex < _columnSize) {
                outer.setPosition(position + sf::Vector2f(x * _nodeSpaceSize + _edgeRadius * 2.0f, y * _nodeSpaceSize + _edgeRadius * 2.0f));

                _rt->draw(outer);

                inner.setPosition(position + sf::Vector2f(x * _nodeSpaceSize + _edgeRadius * 2.0f, y * _nodeSpaceSize + _edgeRadius * 2.0f));

				int tx = cx * _rootColumnSize + x;
				int ty = cy * _rootColumnSize + y;

				bool highlight = (tx == _highlightX && ty == _highlightY);

				if (highlight) {
					_highlightedCSDRPos.x = cx;
					_highlightedCSDRPos.y = cy;
					_highlightedCSDRPos.z = subIndex;
				}

				sf::Color nodeInnerColor = highlight ? _nodeInnerColorHighlight : _nodeInnerColor;
	
                inner.setFillColor(sf::Color(nodeInnerColor.r, nodeInnerColor.g, nodeInnerColor.b, subIndex == index || highlight ? 255 : 0));

                _rt->draw(inner);
            }
		}
}

void CSDRVis::draw() {
    _rt->clear(sf::Color::Transparent);

    float rSize = _nodeSpaceSize * _rootColumnSize;

	// float rWidth = rSize * _width;
	// float rHeight = rSize * _height;

	for (int x = 0; x < _width; x++)
		for (int y = 0; y < _height; y++)
            drawColumn(sf::Vector2f(x * rSize, y * rSize), at(x, y), x % 2 != y % 2, x, y);

    _rt->display();
}
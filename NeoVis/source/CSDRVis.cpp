// ----------------------------------------------------------------------------
//  NeoVis
//  Copyright(c) 2017-2020 Ogma Intelligent Systems Corp. All rights reserved.
//
//  This copy of NeoVis is licensed to you under the terms described
//  in the NEOVIS_LICENSE.md file included in this distribution.
// ----------------------------------------------------------------------------

#include "CSDRVis.h"
#include <cmath>

void CSDRVis::init(int width, int height, int columnSize) {
	width = width;
	height = height;
    columnSize = columnSize;
    rootColumnSize = std::ceil(std::sqrt(static_cast<float>(columnSize)));

	columns.clear();
	columns.assign(width * height, 0);

    float rSize = nodeSpaceSize * rootColumnSize;

    rt.reset(new sf::RenderTexture());

    rt->create(static_cast<int>(std::ceil(width * rSize)), static_cast<int>(std::ceil(height * rSize)));
}

void CSDRVis::drawColumn(const sf::Vector2f &position, int index, bool isOdd, int cx, int cy) {
    float rSize = nodeSpaceSize * rootColumnSize;

    sf::Color backgroundColor = isOdd ? backgroundColor1 : backgroundColor0;

	sf::RectangleShape rsHorizontal;
	rsHorizontal.setPosition(position + sf::Vector2f(0.0f, edgeRadius));
	rsHorizontal.setSize(sf::Vector2f(rSize, rSize - edgeRadius * 2.0f));
	rsHorizontal.setFillColor(backgroundColor);
	
	rt->draw(rsHorizontal);

	sf::RectangleShape rsVertical;
	rsVertical.setPosition(position + sf::Vector2f(edgeRadius, 0.0f));
	rsVertical.setSize(sf::Vector2f(rSize - edgeRadius * 2.0f, rSize));
	rsVertical.setFillColor(backgroundColor);

	rt->draw(rsVertical);

	// Corners
	sf::CircleShape corner;
	corner.setRadius(edgeRadius);
	corner.setPointCount(edgeSegments);
	corner.setFillColor(backgroundColor);
	corner.setOrigin(sf::Vector2f(edgeRadius, edgeRadius));

	corner.setPosition(position + sf::Vector2f(edgeRadius, edgeRadius));
	rt->draw(corner);

	corner.setPosition(position + sf::Vector2f(rSize - edgeRadius, edgeRadius));
	rt->draw(corner);

	corner.setPosition(position + sf::Vector2f(rSize - edgeRadius, rSize - edgeRadius));
	rt->draw(corner);

	corner.setPosition(position + sf::Vector2f(edgeRadius, rSize - edgeRadius));
	rt->draw(corner);

	// Nodes
	sf::CircleShape outer;
	outer.setRadius(nodeSpaceSize * nodeOuterRatio * 0.5f);
	outer.setPointCount(nodeOuterSegments);
	outer.setFillColor(nodeOuterColor);
	outer.setOrigin(sf::Vector2f(outer.getRadius(), outer.getRadius()));

	sf::CircleShape inner;
	inner.setRadius(nodeSpaceSize * nodeOuterRatio * nodeInnerRatio * 0.5f);
	inner.setPointCount(nodeInnerSegments);
	//inner.setFillColor(nodeInnerColor);
	inner.setOrigin(sf::Vector2f(inner.getRadius(), inner.getRadius()));

	for (int x = 0; x < rootColumnSize; x++)
		for (int y = 0; y < rootColumnSize; y++) {
            int subIndex = x + y * rootColumnSize;

            if (subIndex < columnSize) {
                outer.setPosition(position + sf::Vector2f(x * nodeSpaceSize + edgeRadius * 2.0f, y * nodeSpaceSize + edgeRadius * 2.0f));

                rt->draw(outer);

                inner.setPosition(position + sf::Vector2f(x * nodeSpaceSize + edgeRadius * 2.0f, y * nodeSpaceSize + edgeRadius * 2.0f));

				int tx = cx * rootColumnSize + x;
				int ty = cy * rootColumnSize + y;

				bool highlight = (tx == highlightX && ty == highlightY);

				if (highlight) {
					highlightedCSDRPos.x = cx;
					highlightedCSDRPos.y = cy;
					highlightedCSDRPos.z = subIndex;
				}

				sf::Color nodeInnerColor = highlight ? nodeInnerColorHighlight : nodeInnerColor;
	
                inner.setFillColor(sf::Color(nodeInnerColor.r, nodeInnerColor.g, nodeInnerColor.b, subIndex == index || highlight ? 255 : 0));

                rt->draw(inner);
            }
		}
}

void CSDRVis::draw() {
    rt->clear(sf::Color::Transparent);

    float rSize = nodeSpaceSize * rootColumnSize;

	// float rWidth = rSize * width;
	// float rHeight = rSize * height;

	for (int x = 0; x < width; x++)
		for (int y = 0; y < height; y++)
            drawColumn(sf::Vector2f(x * rSize, y * rSize), at(x, y), x % 2 != y % 2, x, y);

    rt->display();
}
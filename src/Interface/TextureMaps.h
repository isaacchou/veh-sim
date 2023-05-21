/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#pragma once

#include <map>

struct Color
{
	unsigned char r;
	unsigned char g;
	unsigned char b;

	Color(const char* html_color_code); // e.g. "Red" or "#FF0000"
	Color(unsigned char r, unsigned char g, unsigned char b) : r(r), g(g), b(b) {}
	size_t hash() const;
};

//~~~
// A collection of 2D images that can be used
// to create OpenGL texture maps
//~~~
class TextureMap
{
protected:
	enum Type {
		ImageFile,
		SolidColor,
		CheckerBoard,
		DiagonalStripes,
		VerticalStripes,
		HorizontalStripes
	};

	struct Image2D
	{	// no need to deep copy
		size_t width, height;
		unsigned char* data;
		Image2D() : width(0), height(0), data(NULL) {}
	};
	std::map<size_t, unsigned int> m_TextureCache; // maps from hash to id
	std::map<int, const Image2D> m_ImageMap;	   // maps from id to Image2D
	int m_next_image_id = 0;
	size_t row_bytes (size_t width) { return ((((width * 3) + 3) >> 2) << 2); }
	unsigned int create(size_t width, size_t height, unsigned char* data, size_t nbytes);

public:
	TextureMap() : m_next_image_id (1000) {}
	virtual ~TextureMap() { for (auto i : m_ImageMap) delete[] i.second.data; }

	const std::map<int, const Image2D>& get_image_map() const { return m_ImageMap; }
	
	unsigned int from_file(const char* texture_path);
	unsigned int solid_color(const Color& clr);
	unsigned int checker_board(size_t width, size_t height, const Color& clr_1, const Color& clr_2);
	// diagonal strip styles:
	// 0: top right to bottom left
	// 1: top left to bottom right
	// 2: top center to bottom left and right
	// 2: bottom center to top left and right
	unsigned int diagonal_stripes(size_t width, size_t height, int stype, const Color& clr_1, const Color& clr_2);
	unsigned int vertical_stripes(size_t height, const Color& clr_1, const Color& clr_2);
	unsigned int horizontal_stripes(size_t width, const Color& clr_1, const Color& clr_2);
};

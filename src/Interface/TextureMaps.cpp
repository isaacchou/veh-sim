/*
 * Copyright (c) 2023 Isaac Chou
 * 
 * This software is licensed under the MIT License that can be 
 * found in the LICENSE file at the top of the source tree
 */
#include <string>
#include <algorithm>
#include <functional>
#include "TextureMaps.h"
#include "../Utils.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

size_t Color::hash() const
{
	std::hash<int> hash_func;
	return hash_func((r << 16) | (g << 8) | b);
}

Color::Color(const char* html_color_code)
{
	std::string s = html_color_code;
	if (s[0] != '#') {
		std::transform(s.begin(), s.end(), s.begin(), std::tolower);
		if (0 == s.compare("black")) {
			s = "#000000";
		} else if (0 == s.compare("white")) {
			s = "#FFFFFF";
		} else if (0 == s.compare("red")) {
			s = "#FF0000";
		} else if (0 == s.compare("green")) {
			s = "#00FF00";
		} else if (0 == s.compare("blue")) {
			s = "#0000FF";
		} else if (0 == s.compare("gray") || 0 == s.compare("grey")) {
			s = "#808080";
		} else if (0 == s.compare("yellow")) {
			s = "#FFFF00";
		} else if (0 == s.compare("gold")) {
			s = "#FFD700";
		} else if (0 == s.compare("orange")) {
			s = "#FFA500";
		} else {
			s = "#000000";
			debug_log("HTML color name (%s) not implemented\n", html_color_code);
		}
	}
	int hex = std::stoi(s.substr(1, 6), 0, 16);
	r = (hex >> 16) & 0xFF;
	g = (hex >> 8) & 0xFF;
	b = hex & 0xFF;
}

unsigned int TextureMap::create(size_t width, size_t height, unsigned char* data, size_t nbytes)
{	// the data should be height * row_bytes which is 4-byte aligned
	size_t n = row_bytes(width);
	Image2D image;
	image.width = width;
	image.height = height;
	image.data = new unsigned char[n * height];
	if (n == nbytes) {
		memcpy(image.data, data, n * height);
	} else {
		unsigned char* p = image.data;
		for (int i = 0; i < height; i++) {
			memcpy(p, data, width * 3);
			p += n;
			data += nbytes;
		}
	}
	m_ImageMap.insert({ m_next_image_id, image });
	return m_next_image_id++;
}

unsigned int TextureMap::from_file(const char* texture_path)
{	// load image, create texture and generate mipmaps
	std::hash<std::string> str_hash_func;
	std::hash<size_t> hash_func;
	size_t h1 = str_hash_func(texture_path);
	size_t h2 = hash_func((size_t)TextureMap::ImageFile);
	size_t hash = hash_func(h1 + h2);
	auto t = m_TextureCache.find(hash);
	if (t != m_TextureCache.end()) return (*t).second;

	int width, height, nrChannels;
	stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
	unsigned char* data = stbi_load(texture_path, &width, &height, &nrChannels, 0);
	if (data == NULL)
	{	// return 0 to draw the object in wireframe mode
		debug_log("Failed to load texture: %s\n", texture_path);
		return 0;
	}
	unsigned int texture = create(width, height, data, width * 3);
	stbi_image_free(data);
	m_TextureCache.insert({ hash, texture });
	return texture;
}

unsigned int TextureMap::solid_color(const Color& clr)
{
	std::hash<size_t> hash_func;
	size_t h1 = clr.hash();
	size_t h2 = hash_func((size_t)TextureMap::SolidColor);
	size_t hash = hash_func(h1 + h2);
	auto t = m_TextureCache.find(hash);
	if (t != m_TextureCache.end()) return (*t).second;

	unsigned char color[4] = { clr.r, clr.g, clr.b, 0 }; // has to be 4-byte aligned
	unsigned int texture = create(1, 1, color, 4);
	m_TextureCache.insert({ hash, texture });
	return texture;
}

unsigned int TextureMap::checker_board(size_t width, size_t height, const Color& clr_1, const Color& clr_2)
{
	std::hash<size_t> hash_func;
	size_t h1 = clr_1.hash();
	size_t h2 = clr_1.hash();
	size_t h3 = hash_func(width);
	size_t h4 = hash_func(height);
	size_t h5 = hash_func((size_t)TextureMap::CheckerBoard);
	size_t hash = hash_func(h1 + h2 + h3 + h4 + h5);
	auto t = m_TextureCache.find(hash);
	if (t != m_TextureCache.end()) return (*t).second;
	
	size_t row_width = row_bytes(width);
	size_t packing = row_width - (width * 3);
	unsigned char* map = new unsigned char[row_width * height];
	unsigned char* p = map;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			const Color& color = ((x + y) % 2) == 0 ? clr_1 : clr_2;
			*p++ = color.r;
			*p++ = color.g;
			*p++ = color.b;
		}
		p += packing;
	}
	unsigned int texture = create(width, height, map, row_width);
	delete[] map;
	m_TextureCache.insert({ hash, texture });
	return texture;
}

unsigned int TextureMap::diagonal_stripes(size_t width, size_t height, int style, const Color& clr_1, const Color& clr_2)
{
	style = style & 3; // use only the last two bits

	std::hash<size_t> hash_func;
	size_t h1 = clr_1.hash();
	size_t h2 = clr_1.hash();
	size_t h3 = hash_func(width);
	size_t h4 = hash_func(height);
	size_t h5 = hash_func(style);
	size_t h6 = hash_func((size_t)TextureMap::DiagonalStripes);
	size_t hash = hash_func(h1 + h2 + h3 + h4 + h5 + h6);
	auto t = m_TextureCache.find(hash);
	if (t != m_TextureCache.end()) return (*t).second;

	size_t row_width = row_bytes(width);
	size_t packing = row_width - (width * 3);
	unsigned char* map = new unsigned char[row_width * height];
	unsigned char* p = map;
	int s = 4; // change color every (1 << 4) pixels
	Color edge((clr_1.r + clr_2.r) / 2, (clr_1.g + clr_2.g) / 2, (clr_1.b + clr_2.b) / 2);
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int t = (style & 2) >> 1;
			int b = style & 1;
			int d = t ? (x >= (width / 2) ? b : (1 - b)) : style;
			int xx = d == 0 ? x : (int)width - x - 1;
			int e = (y + xx) % (2 << s);
			if (e == 0 || e == ((1 << s) - 1)) {
				// soften edge pixels
				*p++ = edge.r;
				*p++ = edge.g;
				*p++ = edge.b;
			} else {
				const Color& color = (((y + xx) >> s) % 2) == 0 ? clr_1 : clr_2;
				*p++ = color.r;
				*p++ = color.g;
				*p++ = color.b;
			}
		}
		p += packing;
	}
	unsigned int texture = create(width, height, map, row_width);
	delete[] map;
	m_TextureCache.insert({ hash, texture });
	return texture;
}

unsigned int TextureMap::vertical_stripes(size_t height, const Color& clr_1, const Color& clr_2)
{
	std::hash<size_t> hash_func;
	size_t h1 = clr_1.hash();
	size_t h2 = clr_1.hash();
	size_t h3 = hash_func(height);
	size_t h4 = hash_func((size_t)TextureMap::VerticalStripes);
	size_t hash = hash_func(h1 + h2 + h3 + h4);
	auto t = m_TextureCache.find(hash);
	if (t != m_TextureCache.end()) return (*t).second;

	unsigned char* map = new unsigned char[4 * (size_t)height]; // row width with packing = 4
	unsigned char* p = map;
	for (int i = 0; i < height; i++) {
		const Color& clr = (i & 1) == 0 ? clr_1 : clr_2;
		p[0] = clr.r;
		p[1] = clr.g;
		p[2] = clr.b;
		p += 4;
	}
	unsigned int texture = create(1, height, map, 4);
	m_TextureCache.insert({ hash, texture });
	return texture;
}

unsigned int TextureMap::horizontal_stripes(size_t width, const Color& clr_1, const Color& clr_2)
{
	std::hash<size_t> hash_func;
	size_t h1 = clr_1.hash();
	size_t h2 = clr_1.hash();
	size_t h3 = hash_func(width);
	size_t h4 = hash_func((size_t)TextureMap::HorizontalStripes);
	size_t hash = hash_func(h1 + h2 + h3 + h4);
	auto t = m_TextureCache.find(hash);
	if (t != m_TextureCache.end()) return (*t).second;

	size_t row_width = row_bytes(width);
	unsigned char* map = new unsigned char[row_width];
	unsigned char* p = map;
	for (int i = 0; i < width; i++) {
		const Color& clr = (i & 1) == 0 ? clr_1 : clr_2;
		*p++ = clr.r;
		*p++ = clr.g;
		*p++ = clr.b;
	}
	unsigned int texture = create(width, 1, map, row_width);
	m_TextureCache.insert({ hash, texture });
	return texture;
}

/*
 * This software is provided as is, without any warranty, express or implied.
 * Copyright 2023 Chris Malnick. All rights reserved.
 */

#include <libftd3xx/ftd3xx.h>
#include <SFML/Graphics.hpp>

#define NAME "xx3dsfml"

#define PRODUCT "N3DSXL"
#define BULK_IN 0x82

#define CAP_WIDTH 240
#define CAP_HEIGHT 720

#define CAP_RES (CAP_WIDTH * CAP_HEIGHT)

#define RGB_FRAME_SIZE (CAP_RES * 3)
#define RGBA_FRAME_SIZE (CAP_RES * 4)

#define TOP_WIDTH 400
#define TOP_HEIGHT 240

#define TOP_RES (TOP_WIDTH * TOP_HEIGHT)

#define BOT_WIDTH 320
#define BOT_HEIGHT 240

#define DELTA_WIDTH (TOP_WIDTH - BOT_WIDTH)
#define DELTA_RES (DELTA_WIDTH * TOP_HEIGHT)

FT_HANDLE handle;
bool connected = false;

void open() {
	if (connected) {
		return;
	}

	if (FT_Create(const_cast<char*>(PRODUCT), FT_OPEN_BY_DESCRIPTION, &handle)) {
		printf("[%s] Open failed.\n", NAME);
		return;
	}

	if (FT_SetStreamPipe(handle, false, false, BULK_IN, RGB_FRAME_SIZE)) {
		printf("[%s] Stream failed.\n", NAME);
		return;
	}

	connected = true;
}

void close() {
	if (!connected) {
		return;
	}

	if (FT_Close(handle)) {
		printf("[%s] Close failed.\n", NAME);
		return;
	}

	connected = false;
}

bool capture(UCHAR *p_buf) {
	ULONG read = 0;

	while (read != RGB_FRAME_SIZE) {
		if (FT_ReadPipe(handle, BULK_IN, p_buf, RGB_FRAME_SIZE, &read, 0)) {
			printf("[%s] Read failed.\n", NAME);
			close();

			return false;
		}

		if (read == 2) {
			return false;
		}
	}

	return true;
}

bool captureEx(UCHAR *p_buf) {
	ULONG read = 0;

	while (read != RGB_FRAME_SIZE) {
		if (FT_ReadPipeEx(handle, 0, p_buf, RGB_FRAME_SIZE, &read, 0)) {
			printf("[%s] Read failed.\n", NAME);
			close();

			return false;
		}

		if (read == 2) {
			return false;
		}
	}

	return true;
}

bool captureAsync(UCHAR *p_buf) {
	OVERLAPPED overlap;
	ULONG read = 0;

	while (read != RGB_FRAME_SIZE) {
		FT_InitializeOverlapped(handle, &overlap);

		if (FT_ReadPipeAsync(handle, 0, p_buf, RGB_FRAME_SIZE, &read, &overlap) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			close();

			return false;
		}

		if (read == 2) {
			return false;
		}

		if (FT_GetOverlappedResult(handle, &overlap, &read, true) == FT_IO_INCOMPLETE && FT_AbortPipe(handle, BULK_IN)) {
			printf("[%s] Abort failed.\n", NAME);
		}

		FT_ReleaseOverlapped(handle, &overlap);
	}

	return true;
}

void map(UCHAR *p_in, UCHAR *p_out) {
	for (int i = 0, j = DELTA_RES, k = TOP_RES; i < CAP_RES; ++i) {
		if (i < DELTA_RES) {
			p_out[4 * i + 0] = p_in[3 * i + 0];
			p_out[4 * i + 1] = p_in[3 * i + 1];
			p_out[4 * i + 2] = p_in[3 * i + 2];
			p_out[4 * i + 3] = 0xff;
		}

		else if (i / CAP_WIDTH & 1) {
			p_out[4 * j + 0] = p_in[3 * i + 0];
			p_out[4 * j + 1] = p_in[3 * i + 1];
			p_out[4 * j + 2] = p_in[3 * i + 2];
			p_out[4 * j + 3] = 0xff;

			++j;
		}

		else {
			p_out[4 * k + 0] = p_in[3 * i + 0];
			p_out[4 * k + 1] = p_in[3 * i + 1];
			p_out[4 * k + 2] = p_in[3 * i + 2];
			p_out[4 * k + 3] = 0xff;

			++k;
		}
	}
}

void render() {
	int win_width = TOP_WIDTH;
	int win_height = TOP_HEIGHT + BOT_HEIGHT;

	int scale = 1;

	int frame = 0;
	int fps = 0;

	UCHAR in_buf[RGB_FRAME_SIZE];
	UCHAR out_buf[RGBA_FRAME_SIZE];

	sf::RenderWindow win(sf::VideoMode(win_width, win_height), std::string(NAME) + " [0 fps]");
	sf::View view(sf::FloatRect(0, 0, win_width, win_height));

	sf::RectangleShape top_rect(sf::Vector2f(TOP_HEIGHT, TOP_WIDTH));
	sf::RectangleShape bot_rect(sf::Vector2f(BOT_HEIGHT, BOT_WIDTH));

	sf::RectangleShape out_rect(sf::Vector2f(win_width, win_height));

	sf::Texture in_tex;
	sf::RenderTexture out_tex;

	sf::Event event;
	sf::Clock clock;

	win.setView(view);

	top_rect.setRotation(-90);
	top_rect.setPosition(0, TOP_HEIGHT);

	bot_rect.setRotation(-90);
	bot_rect.setPosition(DELTA_WIDTH / 2, TOP_HEIGHT + BOT_HEIGHT);

	out_rect.setOrigin(win_width / 2, win_height / 2);
	out_rect.setPosition(win_width / 2, win_height / 2);

	in_tex.create(CAP_WIDTH, CAP_HEIGHT);
	out_tex.create(win_width, win_height);

	top_rect.setTexture(&in_tex);
	top_rect.setTextureRect(sf::IntRect(0, 0, TOP_HEIGHT, TOP_WIDTH));

	bot_rect.setTexture(&in_tex);
	bot_rect.setTextureRect(sf::IntRect(0, TOP_WIDTH, BOT_HEIGHT, BOT_WIDTH));

	out_rect.setTexture(&out_tex.getTexture());

	while (win.isOpen()) {
		while (win.pollEvent(event)) {
			switch (event.type) {
			case sf::Event::Closed:
				win.close();
				break;

			case sf::Event::KeyPressed:
				switch (event.key.code) {
				case sf::Keyboard::Num1:
					connected ? close() : open();
					break;

				case sf::Keyboard::Num2:
					out_tex.setSmooth(!out_tex.isSmooth());
					break;

				case sf::Keyboard::Num3:
					scale -= scale == 1 ? 0 : 1;
					break;

				case sf::Keyboard::Num4:
					scale += scale == 4 ? 0 : 1;
					break;

				case sf::Keyboard::Num5:
					std::swap(win_width, win_height);

					out_rect.setSize(sf::Vector2f(win_width, win_height));
					out_rect.setOrigin(win_width / 2, win_height / 2);
					out_rect.rotate(-90);

					break;

				case sf::Keyboard::Num6:
					std::swap(win_width, win_height);

					out_rect.setSize(sf::Vector2f(win_width, win_height));
					out_rect.setOrigin(win_width / 2, win_height / 2);
					out_rect.rotate(90);

					break;

				default:
					break;
				}

				break;

			default:
				break;
			}
		}

		win.clear();
		win.setSize(sf::Vector2u(win_width * scale, win_height * scale));

		if (connected && captureEx(in_buf)) {
			map(in_buf, out_buf);

			in_tex.update(out_buf, CAP_WIDTH, CAP_HEIGHT, 0, 0);

			out_tex.clear();

			out_tex.draw(top_rect);
			out_tex.draw(bot_rect);

			out_tex.display();

			win.draw(out_rect);

			fps = 1 / clock.restart().asSeconds();
		}

		else {
			clock.restart();
			fps = 0;
		}

		win.display();

		if (++frame % 30 == 0) {
			win.setTitle(std::string(NAME) + " [" + std::to_string(fps) + " fps]");
		}
	}
}

int main() {
	open();
	render();
	close();

	return 0;
}

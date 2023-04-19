/*
 * This software is provided as is, without any warranty, express or implied.
 * Copyright 2023 Chris Malnick. All rights reserved.
 */

#include <libftd3xx/ftd3xx.h>
#include <SFML/Graphics.hpp>
#include <thread>

#define NAME "xx3dsfml"
#define PRODUCT "N3DSXL"

#define BULK_OUT 0x02
#define BULK_IN 0x82

#define FIFO_CHANNEL 0

#define CAP_WIDTH 240
#define CAP_HEIGHT 720

#define CAP_RES (CAP_WIDTH * CAP_HEIGHT)

#define RGB_FRAME_SIZE (CAP_RES * 3)
#define RGBA_FRAME_SIZE (CAP_RES * 4)

#define BUF_COUNT 8
#define BUF_SIZE (RGB_FRAME_SIZE)

#define TOP_WIDTH 400
#define TOP_HEIGHT 240

#define TOP_RES (TOP_WIDTH * TOP_HEIGHT)

#define BOT_WIDTH 320
#define BOT_HEIGHT 240

#define DELTA_WIDTH (TOP_WIDTH - BOT_WIDTH)
#define DELTA_RES (DELTA_WIDTH * TOP_HEIGHT)

#define FRAMERATE 60

FT_HANDLE handle;
bool connected = false;
bool running = true;

UCHAR in_buf[BUF_COUNT][BUF_SIZE];
int curr_buf = 0;

bool open() {
	if (connected) {
		return false;
	}

	if (FT_Create(const_cast<char*>(PRODUCT), FT_OPEN_BY_DESCRIPTION, &handle)) {
		printf("[%s] Create failed.\n", NAME);
		return false;
	}

	UCHAR buf[4] = {0x40, 0x00, 0x00, 0x00};
	ULONG written = 0;

	if (FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0)) {
		printf("[%s] Write failed.\n", NAME);
		return false;
	}

	if (FT_SetStreamPipe(handle, false, false, BULK_IN, BUF_SIZE)) {
		printf("[%s] Stream failed.\n", NAME);
		return false;
	}

	return true;
}

void capture() {
	ULONG read[BUF_COUNT];
	OVERLAPPED overlap[BUF_COUNT];

start:
	while (!connected) {
		if (!running) {
			return;
		}
	}

	for (curr_buf = 0; curr_buf < BUF_COUNT; ++curr_buf) {
		if (FT_InitializeOverlapped(handle, &overlap[curr_buf])) {
			printf("[%s] Initialize failed.\n", NAME);
			goto end;
		}
	}

	for (curr_buf = 0; curr_buf < BUF_COUNT; ++curr_buf) {
		if (FT_ReadPipeAsync(handle, FIFO_CHANNEL, in_buf[curr_buf], BUF_SIZE, &read[curr_buf], &overlap[curr_buf]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			goto end;
		}
	}

	curr_buf = 0;

	while (connected && running) {
		if (FT_GetOverlappedResult(handle, &overlap[curr_buf], &read[curr_buf], true) == FT_IO_INCOMPLETE && FT_AbortPipe(handle, BULK_IN)) {
			printf("[%s] Abort failed.\n", NAME);
			goto end;
		}

		if (FT_ReadPipeAsync(handle, FIFO_CHANNEL, in_buf[curr_buf], BUF_SIZE, &read[curr_buf], &overlap[curr_buf]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			goto end;
		}

		if (++curr_buf == BUF_COUNT) {
			curr_buf = 0;
		}
	}

end:
	for (curr_buf = 0; curr_buf < BUF_COUNT; ++curr_buf) {
		if (FT_ReleaseOverlapped(handle, &overlap[curr_buf])) {
			printf("[%s] Release failed.\n", NAME);
		}
	}

	if (FT_Close(handle)) {
		printf("[%s] Close failed.\n", NAME);
	}

	connected = false;
	goto start;
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
	std::thread thread(capture);

	int win_width = TOP_WIDTH;
	int win_height = TOP_HEIGHT + BOT_HEIGHT;

	int scale = 1;

	UCHAR out_buf[RGBA_FRAME_SIZE];

	sf::RenderWindow win(sf::VideoMode(win_width, win_height), NAME);
	sf::View view(sf::FloatRect(0, 0, win_width, win_height));

	sf::RectangleShape top_rect(sf::Vector2f(TOP_HEIGHT, TOP_WIDTH));
	sf::RectangleShape bot_rect(sf::Vector2f(BOT_HEIGHT, BOT_WIDTH));

	sf::RectangleShape out_rect(sf::Vector2f(win_width, win_height));

	sf::Texture in_tex;
	sf::RenderTexture out_tex;

	sf::Event event;

	win.setFramerateLimit(FRAMERATE + FRAMERATE / 2);
	win.setKeyRepeatEnabled(false);

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
					connected = open();
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

		if (connected) {
			map(in_buf[(curr_buf == 0 ? BUF_COUNT : curr_buf) - 1], out_buf);

			in_tex.update(out_buf, CAP_WIDTH, CAP_HEIGHT, 0, 0);

			out_tex.clear();

			out_tex.draw(top_rect);
			out_tex.draw(bot_rect);

			out_tex.display();

			win.draw(out_rect);
		}

		win.display();
	}

	running = false;
	thread.join();
}

int main() {
	connected = open();
	render();

	return 0;
}

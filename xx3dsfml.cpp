/*
 * This software is provided as is, without any warranty, express or implied.
 * Copyright 2023 Chris Malnick. All rights reserved.
 */

#include <libftd3xx/ftd3xx.h>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <thread>
#include <queue>

#define NAME "xx3dsfml"
#define PRODUCT "N3DSXL"

#define BULK_OUT 0x02
#define BULK_IN 0x82

#define FIFO_CHANNEL 0

#define CAP_WIDTH 240
#define CAP_HEIGHT 720

#define CAP_RES (CAP_WIDTH * CAP_HEIGHT)

#define FRAME_SIZE_RGB (CAP_RES * 3)
#define FRAME_SIZE_RGBA (CAP_RES * 4)

#define AUDIO_CHANNELS 2
#define SAMPLE_RATE 32728

#define SAMPLE_SIZE_8 2192
#define SAMPLE_SIZE_16 (SAMPLE_SIZE_8 / 2)

#define BUF_COUNT 8
#define BUF_SIZE (FRAME_SIZE_RGB + SAMPLE_SIZE_8)

#define TOP_WIDTH 400
#define TOP_HEIGHT 240

#define TOP_RES (TOP_WIDTH * TOP_HEIGHT)

#define BOT_WIDTH 320
#define BOT_HEIGHT 240

#define DELTA_WIDTH (TOP_WIDTH - BOT_WIDTH)
#define DELTA_RES (DELTA_WIDTH * TOP_HEIGHT)

#define FRAMERATE 60
#define AUDIO_LATENCY 4

struct Sample {
	Sample(sf::Int16 *bytes, std::size_t size) : bytes(bytes), size(size) {}

	sf::Int16 *bytes;
	std::size_t size;
};

FT_HANDLE handle;

bool connected = false;
bool running = true;

UCHAR cap_buf[BUF_COUNT][BUF_SIZE];
ULONG read[BUF_COUNT];

int curr_in, curr_out = 0;

std::queue<Sample> samples;

class Audio : public sf::SoundStream {
public:
	Audio() {
		sf::SoundStream::initialize(AUDIO_CHANNELS, SAMPLE_RATE);
		sf::SoundStream::setProcessingInterval(sf::milliseconds(0));
	}

private:
	bool onGetData(sf::SoundStream::Chunk &data) override {
		if (samples.empty()) {
			return false;
		}

		data.samples = samples.front().bytes;
		data.sampleCount = samples.front().size;

		samples.pop();

		return true;
	}

	void onSeek(sf::Time timeOffset) override {}
};

bool open() {
	if (connected) {
		return false;
	}

	if (FT_Create(const_cast<char*>(PRODUCT), FT_OPEN_BY_DESCRIPTION, &handle)) {
		printf("[%s] Create failed.\n", NAME);
		return false;
	}

	UCHAR buf[4] = {0x40, 0x80, 0x00, 0x00};
	ULONG written = 0;

	if (FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0)) {
		printf("[%s] Write failed.\n", NAME);
		return false;
	}

	buf[1] = 0x00;

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
	OVERLAPPED overlap[BUF_COUNT];

start:
	while (!connected) {
		if (!running) {
			return;
		}
	}

	for (curr_in = 0; curr_in < BUF_COUNT; ++curr_in) {
		if (FT_InitializeOverlapped(handle, &overlap[curr_in])) {
			printf("[%s] Initialize failed.\n", NAME);
			goto end;
		}
	}

	for (curr_in = 0; curr_in < BUF_COUNT; ++curr_in) {
		if (FT_ReadPipeAsync(handle, FIFO_CHANNEL, cap_buf[curr_in], BUF_SIZE, &read[curr_in], &overlap[curr_in]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			goto end;
		}
	}

	curr_in = 0;

	while (connected && running) {
		if (FT_GetOverlappedResult(handle, &overlap[curr_in], &read[curr_in], true) == FT_IO_INCOMPLETE && FT_AbortPipe(handle, BULK_IN)) {
			printf("[%s] Abort failed.\n", NAME);
			goto end;
		}

		if (FT_ReadPipeAsync(handle, FIFO_CHANNEL, cap_buf[curr_in], BUF_SIZE, &read[curr_in], &overlap[curr_in]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			goto end;
		}

		if (++curr_in == BUF_COUNT) {
			curr_in = 0;
		}
	}

end:
	for (curr_in = 0; curr_in < BUF_COUNT; ++curr_in) {
		if (FT_ReleaseOverlapped(handle, &overlap[curr_in])) {
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

void map(UCHAR *p_in, sf::Int16 *p_out) {
	for (int i = 0; i < SAMPLE_SIZE_16; ++i) {
		p_out[i] = p_in[i * 2 + 1] << 8 | p_in[i * 2];
	}
}

void render() {
	std::thread thread(capture);

	UCHAR vid_buf[FRAME_SIZE_RGBA];
	sf::Int16 aud_buf[BUF_COUNT][SAMPLE_SIZE_16];

	int prev_out = 0;

	int win_width = TOP_WIDTH;
	int win_height = TOP_HEIGHT + BOT_HEIGHT;

	int scale = 1;

	float volume = 10.0f;
	bool mute = false;

	sf::RenderWindow win(sf::VideoMode(win_width, win_height), NAME);
	sf::View view(sf::FloatRect(0, 0, win_width, win_height));

	sf::RectangleShape top_rect(sf::Vector2f(TOP_HEIGHT, TOP_WIDTH));
	sf::RectangleShape bot_rect(sf::Vector2f(BOT_HEIGHT, BOT_WIDTH));

	sf::RectangleShape out_rect(sf::Vector2f(win_width, win_height));

	sf::Texture in_tex;
	sf::RenderTexture out_tex;

	sf::Event event;

	Audio audio;

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

	audio.setVolume(volume);

	while (win.isOpen()) {
		while (win.pollEvent(event)) {
			switch (event.type) {
			case sf::Event::Closed:
				win.close();
				audio.stop();

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

				case sf::Keyboard::Num0:
					mute ^= true;
					audio.setVolume(mute ? 0.0f : volume);

					break;

				case sf::Keyboard::Hyphen:
					volume -= volume == 0.0f ? 0.0f : 5.0f;
					audio.setVolume(volume);

					break;

				case sf::Keyboard::Equal:
					volume += volume == 100.0f ? 0.0f : 5.0f;
					audio.setVolume(volume);

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
			curr_out = (curr_in == 0 ? BUF_COUNT : curr_in) - 1;

			if (curr_out != prev_out) {
				map(cap_buf[curr_out], vid_buf);

				in_tex.update(vid_buf, CAP_WIDTH, CAP_HEIGHT, 0, 0);

				out_tex.clear();

				out_tex.draw(top_rect);
				out_tex.draw(bot_rect);

				out_tex.display();

				if (samples.size() < AUDIO_LATENCY) {
					map(&cap_buf[curr_out][FRAME_SIZE_RGB], aud_buf[curr_out]);
					samples.emplace(aud_buf[curr_out], (read[curr_out] - FRAME_SIZE_RGB) / 2);
				}

				prev_out = curr_out;
			}

			win.draw(out_rect);

			if (audio.getStatus() != sf::SoundStream::Playing) {
				audio.play();
			}
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

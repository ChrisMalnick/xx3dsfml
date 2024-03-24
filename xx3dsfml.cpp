/*
 * This software is provided as is, without any warranty, express or implied.
 * Copyright 2023 Chris Malnick. All rights reserved.
 */

#include <libftd3xx/ftd3xx.h>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <cstring>

#include <fstream>
#include <sstream>

#include <thread>
#include <queue>

#define NAME "xx3dsfml"

#define PRODUCT_1 "N3DSXL"
#define PRODUCT_2 "N3DSXL.2"

#define BULK_OUT 0x02
#define BULK_IN 0x82

#define FIFO_CHANNEL 0

#define TOP_WIDTH_3DS 400
#define BOT_WIDTH_3DS 320

#define HEIGHT_3DS 240

#define CAP_WIDTH HEIGHT_3DS
#define CAP_HEIGHT (TOP_WIDTH_3DS + BOT_WIDTH_3DS)

#define CAP_RES (CAP_WIDTH * CAP_HEIGHT)

#define FRAME_SIZE_RGB (CAP_RES * 3)
#define FRAME_SIZE_RGBA (CAP_RES * 4)

#define AUDIO_CHANNELS 2
#define SAMPLE_RATE 32734

#define SAMPLE_SIZE_8 2192
#define SAMPLE_SIZE_16 (SAMPLE_SIZE_8 / 2)

#define BUF_COUNT 8
#define BUF_SIZE (FRAME_SIZE_RGB + SAMPLE_SIZE_8)

#define TOP_RES_3DS (TOP_WIDTH_3DS * HEIGHT_3DS)

#define DELTA_WIDTH_3DS (TOP_WIDTH_3DS - BOT_WIDTH_3DS)
#define DELTA_RES_3DS (DELTA_WIDTH_3DS * HEIGHT_3DS)

#define WIDTH_DS 256
#define HEIGHT_DS 192

#define DELTA_HEIGHT_DS (HEIGHT_3DS - HEIGHT_DS)

enum Crop { DEFAULT_3DS, SCALED_DS, NATIVE_DS };

struct Sample {
	Sample(sf::Int16 *bytes, std::size_t size) : bytes(bytes), size(size) {}

	sf::Int16 *bytes;
	std::size_t size;
};

FT_HANDLE g_handle;

UCHAR g_in_buf[BUF_COUNT][BUF_SIZE];
ULONG g_read[BUF_COUNT];

sf::Texture g_in_tex;
std::queue<Sample> g_samples;

int g_curr_in = 0;
bool g_running = true;

bool g_split, g_swap = false;

int g_volume = 50;
bool g_mute = false;

bool g_init = true;
bool g_skip_io = false;

bool *gp_top_blur, *gp_bot_blur, *gp_joint_blur;
Crop *gp_top_crop, *gp_bot_crop, *gp_joint_crop;
int *gp_top_rotation, *gp_bot_rotation, *gp_joint_rotation;
double *gp_top_scale, *gp_bot_scale, *gp_joint_scale;

class Audio : public sf::SoundStream {
public:
	Audio() {
		sf::SoundStream::initialize(AUDIO_CHANNELS, SAMPLE_RATE);
		sf::SoundStream::setProcessingInterval(sf::seconds(0));
	}

private:
	bool onGetData(sf::SoundStream::Chunk &data) override {
		if (g_samples.empty()) {
			return false;
		}

		data.samples = g_samples.front().bytes;
		data.sampleCount = g_samples.front().size;

		g_samples.pop();

		return true;
	}

	void onSeek(sf::Time timeOffset) override {}
};

Audio g_audio;

bool load(std::string name) {
	std::ifstream file(name);
	std::string line;

	if (!file.good()) {
		printf("[%s] File \"%s\" load failed.\n", NAME, name.c_str());
		return false;
	}

	while (std::getline(file, line)) {
		std::istringstream kvp(line);
		std::string key;

		if (std::getline(kvp, key, '=')) {
			std::string value;

			if (std::getline(kvp, value)) {
				if (key == "bot_blur") {
					*gp_bot_blur = std::stoi(value);
					continue;
				}

				if (key == "bot_crop") {
					*gp_bot_crop = static_cast<Crop>(std::stoi(value));
					continue;
				}

				if (key == "bot_rotation") {
					*gp_bot_rotation = std::stoi(value);
					continue;
				}

				if (key == "bot_scale") {
					*gp_bot_scale = std::stod(value);
					continue;
				}

				if (key == "joint_blur") {
					*gp_joint_blur = std::stoi(value);
					continue;
				}

				if (key == "joint_crop") {
					*gp_joint_crop = static_cast<Crop>(std::stoi(value));
					continue;
				}

				if (key == "joint_rotation") {
					*gp_joint_rotation = std::stoi(value);
					continue;
				}

				if (key == "joint_scale") {
					*gp_joint_scale = std::stod(value);
					continue;
				}

				if (key == "mute") {
					g_mute = std::stoi(value);
					continue;
				}

				if (key == "split") {
					g_split = std::stoi(value);
					continue;
				}

				if (key == "top_blur") {
					*gp_top_blur = std::stoi(value);
					continue;
				}

				if (key == "top_crop") {
					*gp_top_crop = static_cast<Crop>(std::stoi(value));
					continue;
				}

				if (key == "top_rotation") {
					*gp_top_rotation = std::stoi(value);
					continue;
				}

				if (key == "top_scale") {
					*gp_top_scale = std::stod(value);
					continue;
				}

				if (key == "volume") {
					g_volume = std::stoi(value);
					continue;
				}
			}
		}
	}

	file.close();
	return true;
}

void save(std::string name) {
	std::ofstream file(name);

	if (!file.good()) {
		printf("[%s] File \"%s\" save failed.\n", NAME, name.c_str());
		return;
	}

	file << "bot_blur=" << *gp_bot_blur << std::endl;
	file << "bot_crop=" << *gp_bot_crop << std::endl;
	file << "bot_rotation=" << *gp_bot_rotation << std::endl;
	file << "bot_scale=" << *gp_bot_scale << (*gp_bot_scale - static_cast<int>(*gp_bot_scale) ? "" : ".0") << std::endl;
	file << "joint_blur=" << *gp_joint_blur << std::endl;
	file << "joint_crop=" << *gp_joint_crop << std::endl;
	file << "joint_rotation=" << *gp_joint_rotation << std::endl;
	file << "joint_scale=" << *gp_joint_scale << (*gp_joint_scale - static_cast<int>(*gp_joint_scale) ? "" : ".0") << std::endl;
	file << "mute=" << g_mute << std::endl;
	file << "split=" << g_split << std::endl;
	file << "top_blur=" << *gp_top_blur << std::endl;
	file << "top_crop=" << *gp_top_crop << std::endl;
	file << "top_rotation=" << *gp_top_rotation << std::endl;
	file << "top_scale=" << *gp_top_scale << (*gp_top_scale - static_cast<int>(*gp_top_scale) ? "" : ".0") << std::endl;
	file << "volume=" << g_volume << std::endl;

	file.close();
}

class Screen {
public:
	enum Type { TOP, BOT, JOINT };

	sf::RectangleShape m_in_rect;
	sf::RenderWindow m_win;

	Screen(bool **pp_blur, Crop **pp_crop, int **pp_rotation, double **pp_scale) {
		*pp_blur = &this->m_blur;
		*pp_crop = &this->m_crop;
		*pp_rotation = &this->m_rotation;
		*pp_scale = &this->m_scale;
	}

	void build(Screen::Type type, int u, int width, bool open) {
		this->m_type = type;

		this->resize(TOP_WIDTH_3DS, this->height(HEIGHT_3DS));

		this->m_in_rect.setTexture(&g_in_tex);
		this->m_in_rect.setTextureRect(sf::IntRect(0, u, this->m_height, width));

		this->m_in_rect.setSize(sf::Vector2f(this->m_height, width));
		this->m_in_rect.setOrigin(this->m_height / 2, width / 2);

		this->m_in_rect.setRotation(-90);
		this->m_in_rect.setPosition(width / 2, this->m_height / 2);

		this->m_out_tex.create(width, this->m_height);

		this->m_out_rect.setSize(sf::Vector2f(width, this->m_height));
		this->m_out_rect.setTexture(&this->m_out_tex.getTexture());

		this->m_view.reset(sf::FloatRect(0, 0, width, this->m_height));

		if (open) {
			this->open();
		}
	}

	void reset() {
		this->resize(TOP_WIDTH_3DS, this->height(HEIGHT_3DS));

		this->m_view.setRotation(0);

		this->m_view.setSize(this->m_width, this->m_height);
		this->m_win.setView(this->m_view);

		this->m_out_tex.setSmooth(this->m_blur);

		if (this->m_rotation) {
			if (this->horizontal()) {
				std::swap(this->m_width, this->m_height);
			}

			this->rotate();
		}

		if (this->m_crop) {
			this->crop();
		}

		this->move();
	}

	void move() {
		this->m_in_rect.setPosition(this->m_in_rect.getOrigin().y, this->m_in_rect.getOrigin().x);

		switch (this->m_type) {
		case Screen::Type::TOP:
			if (g_split && this->m_crop == Crop::NATIVE_DS) {
				this->m_in_rect.move(0, -DELTA_HEIGHT_DS / 2);
			}

			return;

		case Screen::Type::BOT:
			if (g_split) {
				if (this->m_crop == Crop::NATIVE_DS) {
					this->m_in_rect.move(0, DELTA_HEIGHT_DS / 2);
				}
			}

			else {
				this->m_in_rect.move(DELTA_WIDTH_3DS / 2, HEIGHT_3DS);
			}

			return;
		}
	}

	void poll() {
		while (this->m_win.pollEvent(this->m_event)) {
			switch (this->m_event.type) {
			case sf::Event::Closed:
				g_running = false;
				break;

			case sf::Event::KeyPressed:
				switch (this->m_event.key.code) {
				case sf::Keyboard::S:
					g_split ^= g_swap = true;
					break;

				case sf::Keyboard::C:
					this->m_crop = static_cast<Crop>((this->m_crop + 1) % (Crop::NATIVE_DS + 1));

					this->crop();
					this->move();

					break;

				case sf::Keyboard::B:
					this->m_out_tex.setSmooth(this->m_blur ^= true);
					break;

				case sf::Keyboard::Hyphen:
					this->m_scale -= this->m_scale == 1.0 ? 0.0 : 0.5;
					break;

				case sf::Keyboard::Equal:
					this->m_scale += this->m_scale == 4.5 ? 0.0 : 0.5;
					break;

				case sf::Keyboard::LBracket:
					std::swap(this->m_width, this->m_height);

					this->m_rotation = ((this->m_rotation + 90) % 360 + 360) % 360;
					this->rotate();

					break;

				case sf::Keyboard::RBracket:
					std::swap(this->m_width, this->m_height);

					this->m_rotation = ((this->m_rotation - 90) % 360 + 360) % 360;
					this->rotate();

					break;

				case sf::Keyboard::M:
					g_audio.setVolume((g_mute ^= true) ? 0 : g_volume);
					break;

				case sf::Keyboard::Comma:
					g_audio.setVolume(g_volume -= g_volume == 0 ? 0 : 5);
					break;

				case sf::Keyboard::Period:
					g_audio.setVolume(g_volume += g_volume == 100 ? 0 : 5);
					break;

				case sf::Keyboard::F1:
				case sf::Keyboard::F2:
				case sf::Keyboard::F3:
				case sf::Keyboard::F4:
					if (!g_skip_io && (g_init = load("./presets/layout" + std::to_string(this->m_event.key.code - sf::Keyboard::F1 + 1) + ".conf"))) {
						g_audio.setVolume(g_mute ? 0 : g_volume);
					}

					break;

				case sf::Keyboard::F5:
				case sf::Keyboard::F6:
				case sf::Keyboard::F7:
				case sf::Keyboard::F8:
					if (!g_skip_io) {
						save("./presets/layout" + std::to_string(this->m_event.key.code - sf::Keyboard::F5 + 1) + ".conf");
					}

					break;
				}

				break;
			}
		}
	}

	void toggle() {
		this->m_win.isOpen() ? this->m_win.close() : this->open();
	}

	void draw() {
		this->m_win.setSize(sf::Vector2u(this->m_width * this->m_scale, this->m_height * this->m_scale));

		this->m_out_tex.clear();
		this->m_out_tex.draw(this->m_in_rect);
		this->m_out_tex.display();

		this->m_win.clear();
		this->m_win.draw(this->m_out_rect);
		this->m_win.display();
	}

	void draw(sf::RectangleShape *p_top_rect, sf::RectangleShape *p_bot_rect) {
		this->m_win.setSize(sf::Vector2u(this->m_width * this->m_scale, this->m_height * this->m_scale));

		this->m_out_tex.clear();
		this->m_out_tex.draw(*p_top_rect);
		this->m_out_tex.draw(*p_bot_rect);
		this->m_out_tex.display();

		this->m_win.clear();
		this->m_win.draw(this->m_out_rect);
		this->m_win.display();
	}

private:
	bool m_blur = false;
	Crop m_crop = Crop::DEFAULT_3DS;
	int m_rotation = 0;
	double m_scale = 1.0;

	Screen::Type m_type;
	int m_width, m_height;

	sf::RenderTexture m_out_tex;
	sf::RectangleShape m_out_rect;

	sf::View m_view;
	sf::Event m_event;

	bool horizontal() {
		return this->m_rotation / 10 % 2;
	}

	int height(int height) {
		return height * (this->m_type == Screen::Type::JOINT ? 2 : 1);
	}

	std::string title() {
		switch (this->m_type) {
		case Screen::Type::TOP:
			return std::string(NAME) + "-top";

		case Screen::Type::BOT:
			return std::string(NAME) + "-bot";

		default:
			return NAME;
		}
	}

	void resize(int width, int height) {
		this->m_width = width;
		this->m_height = height;
	}

	void open() {
		this->m_win.create(sf::VideoMode(this->m_width * this->m_scale, this->m_height * this->m_scale), this->title());
		this->m_win.setView(this->m_view);
	}

	void rotate() {
		this->m_view.setRotation(this->m_rotation);

		this->m_view.setSize(this->m_width, this->m_height);
		this->m_win.setView(this->m_view);
	}

	void crop() {
		switch (this->m_crop) {
		case Crop::DEFAULT_3DS:
			this->horizontal() ? this->resize(this->height(HEIGHT_3DS), TOP_WIDTH_3DS) : this->resize(TOP_WIDTH_3DS, this->height(HEIGHT_3DS));
			break;

		case Crop::SCALED_DS:
			this->horizontal() ? this->resize(this->height(HEIGHT_3DS), BOT_WIDTH_3DS) : this->resize(BOT_WIDTH_3DS, this->height(HEIGHT_3DS));
			break;

		case Crop::NATIVE_DS:
			this->horizontal() ? this->resize(this->height(HEIGHT_DS), WIDTH_DS) : this->resize(WIDTH_DS, this->height(HEIGHT_DS));
			break;
		}

		this->m_view.setSize(this->m_width, this->m_height);
		this->m_win.setView(this->m_view);
	}
};

bool open() {
	if (FT_Create(const_cast<char*>(PRODUCT_1), FT_OPEN_BY_DESCRIPTION, &g_handle)) {
		if (FT_Create(const_cast<char*>(PRODUCT_2), FT_OPEN_BY_DESCRIPTION, &g_handle)) {
			printf("[%s] Create failed.\n", NAME);
			return false;
		}
	}

	UCHAR buf[4] = {0x40, 0x80, 0x00, 0x00};
	ULONG written = 0;

	if (FT_WritePipe(g_handle, BULK_OUT, buf, 4, &written, 0)) {
		printf("[%s] Write failed.\n", NAME);
		return false;
	}

	buf[1] = 0x00;

	if (FT_WritePipe(g_handle, BULK_OUT, buf, 4, &written, 0)) {
		printf("[%s] Write failed.\n", NAME);
		return false;
	}

	if (FT_SetStreamPipe(g_handle, false, false, BULK_IN, BUF_SIZE)) {
		printf("[%s] Stream failed.\n", NAME);
		return false;
	}

	return true;
}

void capture() {
	OVERLAPPED overlap[BUF_COUNT];

	for (g_curr_in = 0; g_curr_in < BUF_COUNT; ++g_curr_in) {
		if (FT_InitializeOverlapped(g_handle, &overlap[g_curr_in])) {
			printf("[%s] Initialize failed.\n", NAME);
			goto end;
		}
	}

	g_curr_in = 0;

	while (g_running) {
		memset(g_in_buf[g_curr_in], 0, BUF_SIZE);
		g_read[g_curr_in] = 0;

		if (FT_GetOverlappedResult(g_handle, &overlap[g_curr_in], &g_read[g_curr_in], true) == FT_IO_INCOMPLETE && FT_AbortPipe(g_handle, BULK_IN)) {
			printf("[%s] Abort failed.\n", NAME);
			goto end;
		}

		if (FT_ReadPipeAsync(g_handle, FIFO_CHANNEL, g_in_buf[g_curr_in], BUF_SIZE, &g_read[g_curr_in], &overlap[g_curr_in]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			goto end;
		}

		g_curr_in = (g_curr_in + 1) % BUF_COUNT;
	}

end:
	for (g_curr_in = 0; g_curr_in < BUF_COUNT; ++g_curr_in) {
		if (FT_ReleaseOverlapped(g_handle, &overlap[g_curr_in])) {
			printf("[%s] Release failed.\n", NAME);
		}
	}

	if (FT_Close(g_handle)) {
		printf("[%s] Close failed.\n", NAME);
	}

	g_running = false;
}

void map(UCHAR *p_in, UCHAR *p_out) {
	for (int i = 0, j = DELTA_RES_3DS, k = TOP_RES_3DS; i < CAP_RES; ++i) {
		if (i < DELTA_RES_3DS) {
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

void playback() {
	sf::Int16 out_buf[BUF_COUNT][SAMPLE_SIZE_16];
	int curr_out, prev_out = 0;

	g_audio.setVolume(g_mute ? 0 : g_volume);

	while (g_running) {
		curr_out = (g_curr_in - 1 + BUF_COUNT) % BUF_COUNT;

		if (curr_out != prev_out) {
			if (g_read[curr_out] > FRAME_SIZE_RGB) {
				map(&g_in_buf[curr_out][FRAME_SIZE_RGB], out_buf[curr_out]);
				g_samples.emplace(out_buf[curr_out], (g_read[curr_out] - FRAME_SIZE_RGB) / 2);

				if (g_audio.getStatus() != sf::SoundStream::Playing) {
					g_audio.play();
				}
			}

			prev_out = curr_out;
		}

		sf::sleep(sf::seconds(0));
	}

	g_audio.stop();
}

void render() {
	UCHAR out_buf[FRAME_SIZE_RGBA];
	int curr_out, prev_out = 0;

	g_in_tex.create(CAP_WIDTH, CAP_HEIGHT);

	Screen top_screen(&gp_top_blur, &gp_top_crop, &gp_top_rotation, &gp_top_scale);
	Screen bot_screen(&gp_bot_blur, &gp_bot_crop, &gp_bot_rotation, &gp_bot_scale);
	Screen joint_screen(&gp_joint_blur, &gp_joint_crop, &gp_joint_rotation, &gp_joint_scale);

	if (!g_skip_io) {
		g_skip_io = !load("./" + std::string(NAME) + ".conf");
	}

	top_screen.build(Screen::Type::TOP, 0, TOP_WIDTH_3DS, g_split);
	bot_screen.build(Screen::Type::BOT, TOP_WIDTH_3DS, BOT_WIDTH_3DS, g_split);
	joint_screen.build(Screen::Type::JOINT, 0, TOP_WIDTH_3DS, !g_split);

	std::thread thread(playback);

	while (g_running) {
		top_screen.poll();
		bot_screen.poll();
		joint_screen.poll();

		curr_out = (g_curr_in - 1 + BUF_COUNT) % BUF_COUNT;

		if (curr_out != prev_out) {
			map(g_in_buf[curr_out], out_buf);
			g_in_tex.update(out_buf, CAP_WIDTH, CAP_HEIGHT, 0, 0);

			if (g_init) {
				top_screen.reset();
				bot_screen.reset();
				joint_screen.reset();

				if (!(joint_screen.m_win.isOpen() ^ g_split)) {
					top_screen.toggle();
					bot_screen.toggle();
					joint_screen.toggle();
				}

				g_init = false;
			}

			if (g_swap) {
				top_screen.toggle();
				bot_screen.toggle();
				joint_screen.toggle();

				top_screen.move();
				bot_screen.move();

				g_swap = false;
			}

			if (g_split) {
				top_screen.draw();
				bot_screen.draw();
			}

			else {
				joint_screen.draw(&top_screen.m_in_rect, &bot_screen.m_in_rect);
			}

			prev_out = curr_out;
		}

		sf::sleep(sf::seconds(0));
	}

	thread.join();

	top_screen.m_win.close();
	bot_screen.m_win.close();
	joint_screen.m_win.close();

	if (!g_skip_io) {
		save("./" + std::string(NAME) + ".conf");
	}
}

int main(int argc, char **argv) {
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--safe") == 0) {
			g_skip_io = true;
			continue;
		}

		printf("[%s] Invalid argument \"%s\".\n", NAME, argv[i]);
	}

	if (!open()) {
		return -1;
	}

	std::thread thread(capture);

	render();
	thread.join();

	return 0;
}

/*
 * This software is provided as is, without any warranty, express or implied.
 * Copyright 2023 Chris Malnick. All rights reserved.
 */

#include <libftd3xx/ftd3xx.h>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <cstring>
#include <filesystem>

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

// This isn't precise, however we can use it...
#define USB_FPS 60

#define CAP_RES (CAP_WIDTH * CAP_HEIGHT)

#define FRAME_SIZE_RGB (CAP_RES * 3)
#define FRAME_SIZE_RGBA (CAP_RES * 4)

#define AUDIO_BUF_COUNT 7
#define AUDIO_CHANNELS 2
#define SAMPLE_RATE 32734
#define AUDIO_LATENCY 4
#define AUDIO_NUM_CHECKS 3
#define AUDIO_FAILURE_THRESHOLD 8
#define AUDIO_CHECKS_PER_SECOND ((USB_FPS + (USB_FPS / 12)) * AUDIO_NUM_CHECKS)

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

#define FRAMERATE_LIMIT 60

//#define CLOSE_ON_FAIL
//#define SAFER_QUIT
//#define TRY_HANDLE_BAD_READS

enum Crop { DEFAULT_3DS, SCALED_DS, NATIVE_DS, END };

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

std::string g_conf_dir = std::string(std::getenv("HOME")) + "/.config/" + std::string(NAME);

int g_curr_in = 0;

bool g_connected = false;
bool g_running = true;
bool g_close_success = true;

bool g_split, g_swap = false;
bool g_init = true;

bool g_skip_io = false, g_vsync = false;

int g_volume = 50;
bool g_mute = false;

bool *gp_top_blur, *gp_bot_blur, *gp_joint_blur;
Crop *gp_top_crop, *gp_bot_crop, *gp_joint_crop;
int *gp_top_rotation, *gp_bot_rotation, *gp_joint_rotation;
double *gp_top_scale, *gp_bot_scale, *gp_joint_scale;

bool connect(bool);

class Audio : public sf::SoundStream {
public:
	int loaded_volume = -1;
	bool loaded_mute = false;

	Audio() {
		sf::SoundStream::initialize(AUDIO_CHANNELS, SAMPLE_RATE);
		#if (SFML_VERSION_MAJOR > 2) || ((SFML_VERSION_MAJOR == 2) && (SFML_VERSION_MINOR >= 6))
		sf::SoundStream::setProcessingInterval(sf::milliseconds(0));
		#endif
	}
	
	void update_volume(int volume, bool mute) {
		if(mute && (mute == loaded_mute)) {
			return;
		}
		if(mute != loaded_mute) {
			loaded_mute = mute;
			loaded_volume = volume;
		}
		else if(volume != loaded_volume) {
			loaded_mute = mute;
			loaded_volume = volume;
		}
		else {
			return;
		}
		setVolume(mute ? 0 : volume);
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

bool load(std::string path, std::string name) {
	std::ifstream file(path + name);
	std::string line;

	if (!file.good()) {
		printf("[%s] File \"%s\" load failed.\n", NAME, name.c_str());
		file.close();

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

void save(std::string path, std::string name) {
	std::filesystem::create_directories(path);
	std::ofstream file(path + name);

	if (!file.good()) {
		printf("[%s] File \"%s\" save failed.\n", NAME, name.c_str());
		file.close();

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
				case sf::Keyboard::C:
					g_connected = connect(true);
					break;

				case sf::Keyboard::S:
					g_split ^= g_swap = true;
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

					this->m_rotation = (this->m_rotation + 90) % 360;
					this->rotate();

					break;

				case sf::Keyboard::RBracket:
					std::swap(this->m_width, this->m_height);

					this->m_rotation = ((this->m_rotation - 90) % 360 + 360) % 360;
					this->rotate();

					break;

				case sf::Keyboard::Apostrophe:
					this->m_crop = static_cast<Crop>((this->m_crop + 1) % Crop::END);

					this->crop();
					this->move();

					break;

				case sf::Keyboard::Semicolon:
					this->m_crop = static_cast<Crop>(((this->m_crop - 1) % Crop::END + Crop::END) % Crop::END);

					this->crop();
					this->move();

					break;

				case sf::Keyboard::M:
					g_mute = !g_mute;
					break;

				case sf::Keyboard::Comma:
					g_mute = false;
					g_volume -= 5;
					if(g_volume < 0)
						g_volume = 0;
					break;

				case sf::Keyboard::Period:
					g_mute = false;
					g_volume += 5;
					if(g_volume > 100)
						g_volume = 100;
					break;

				case sf::Keyboard::F1:
				case sf::Keyboard::F2:
				case sf::Keyboard::F3:
				case sf::Keyboard::F4:
					if(!g_skip_io) {
						g_init = load(g_conf_dir + "/presets/", "layout" + std::to_string(this->m_event.key.code - sf::Keyboard::F1 + 1) + ".conf");
					}

					break;

				case sf::Keyboard::F5:
				case sf::Keyboard::F6:
				case sf::Keyboard::F7:
				case sf::Keyboard::F8:
					if (!g_skip_io) {
						save(g_conf_dir + "/presets/", "layout" + std::to_string(this->m_event.key.code - sf::Keyboard::F5 + 1) + ".conf");
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

		g_vsync ? this->m_win.setVerticalSyncEnabled(true) : this->m_win.setFramerateLimit(FRAMERATE_LIMIT);
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

bool connect(bool print_failed) {
	if (g_connected) {
		g_close_success = false;
		return false;
	}
	
	if(!g_close_success) {
		return false;
	}

	if (FT_Create(const_cast<char*>(PRODUCT_1), FT_OPEN_BY_DESCRIPTION, &g_handle)) {
		if (FT_Create(const_cast<char*>(PRODUCT_2), FT_OPEN_BY_DESCRIPTION, &g_handle)) {
			if(print_failed) {
				printf("[%s] Create failed.\n", NAME);
			}
			return false;
		}
	}

	UCHAR buf[4] = {0x40, 0x80, 0x00, 0x00};
	ULONG written = 0;

	if (FT_WritePipe(g_handle, BULK_OUT, buf, 4, &written, 0)) {
		if(print_failed) {
			printf("[%s] Write failed.\n", NAME);
		}
		return false;
	}

	buf[1] = 0x00;

	if (FT_WritePipe(g_handle, BULK_OUT, buf, 4, &written, 0)) {
		if(print_failed) {
			printf("[%s] Write failed.\n", NAME);
		}
		return false;
	}

	if (FT_SetStreamPipe(g_handle, false, false, BULK_IN, BUF_SIZE)) {
		if(print_failed) {
			printf("[%s] Stream failed.\n", NAME);
		}
		return false;
	}

	return true;
}

void bad_read_close() {
	g_close_success = false;
	g_connected = false;
	#ifdef TRY_HANDLE_BAD_READS
	g_close_success = true;
	// Due to a mistake in how FT_ReadPipeAsync works,
	// you need to re-create a device before you can close this down... :/
	// Though the create method itself can also get stuck if you insert
	// the device back at the wrong moment! This is honestly madness... :/
	// The right way to do this would be asking the user to press a key
	// once the device has been re-connected properly... :/
	// The only saving grace is that this type of error while closing is
	// somewhat rare, due to the high amount of time spent in FT_ReadPipeAsync.
	// Still...
	printf("[%s] Please reconnect the console so the program may be properly closed!\n", NAME);
	while(!g_connected) {
		sf::sleep(sf::milliseconds(100));
		g_connected = connect(false);
	}
	#endif
}

void capture() {
	OVERLAPPED overlap[BUF_COUNT];
	FT_STATUS ftStatus;

start:
	bool bad_close = false;
	while (!g_connected) {
		if (!g_running) {
			return;
		}
	}

	for (g_curr_in = 0; g_curr_in < BUF_COUNT; ++g_curr_in) {
		ftStatus = FT_InitializeOverlapped(g_handle, &overlap[g_curr_in]);
		if (ftStatus) {
			printf("[%s] Initialize failed.\n", NAME);
			bad_close = true;
			goto end;
		}
	}

	#ifndef SAFER_QUIT
	for (g_curr_in = 0; g_curr_in < BUF_COUNT; ++g_curr_in) {
		ftStatus = FT_ReadPipeAsync(g_handle, FIFO_CHANNEL, g_in_buf[g_curr_in], BUF_SIZE, &g_read[g_curr_in], &overlap[g_curr_in]);
		if (ftStatus != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			bad_read_close();
			bad_close = true;
			goto end;
		}
	}
	#endif

	g_curr_in = 0;

	while (g_connected && g_running) {
		#ifndef SAFER_QUIT
		ftStatus = FT_GetOverlappedResult(g_handle, &overlap[g_curr_in], &g_read[g_curr_in], true);
		if(FT_FAILED(ftStatus)) {
			printf("[%s] USB error.\n", NAME);
			bad_close = true;
			goto end;
		}
		if (ftStatus == FT_IO_INCOMPLETE && FT_AbortPipe(g_handle, BULK_IN)) {
			printf("[%s] Abort failed.\n", NAME);
			goto end;
		}
		#endif
		ftStatus = FT_ReadPipeAsync(g_handle, FIFO_CHANNEL, g_in_buf[g_curr_in], BUF_SIZE, &g_read[g_curr_in], &overlap[g_curr_in]);
		if (ftStatus != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			bad_read_close();
			bad_close = true;
			goto end;
		}

		#ifdef SAFER_QUIT
		ftStatus = FT_GetOverlappedResult(g_handle, &overlap[g_curr_in], &g_read[g_curr_in], true);
		if(FT_FAILED(ftStatus)) {
			printf("[%s] USB error.\n", NAME);
			bad_close = true;
			goto end;
		}
		if (ftStatus == FT_IO_INCOMPLETE && FT_AbortPipe(g_handle, BULK_IN)) {
			printf("[%s] Abort failed.\n", NAME);
			goto end;
		}
		#endif

		if (++g_curr_in == BUF_COUNT) {
			g_curr_in = 0;
		}
	}
end:
	g_close_success = false;
	g_connected = false;
	for (g_curr_in = 0; g_curr_in < BUF_COUNT; ++g_curr_in) {
		if(!bad_close) {
			ftStatus = FT_GetOverlappedResult(g_handle, &overlap[g_curr_in], &g_read[g_curr_in], true);
			if(FT_FAILED(ftStatus))
				bad_close = true;
		}
		if (FT_ReleaseOverlapped(g_handle, &overlap[g_curr_in])) {
			printf("[%s] Release failed.\n", NAME);
		}
	}

	if (FT_Close(g_handle)) {
		printf("[%s] Close failed.\n", NAME);
	}

	g_close_success = false;
	g_connected = false;

	#ifdef CLOSE_ON_FAIL
	g_running = false;
	#endif
	g_close_success = true;
	goto start;
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

void map(UCHAR *p_in, sf::Int16 *p_out, int n_samples) {
	for (int i = 0; i < n_samples; ++i) {
		p_out[i] = p_in[i * 2 + 1] << 8 | p_in[i * 2];
	}
}

void playback() {
	Audio audio;
	sf::Int16 out_buf[AUDIO_BUF_COUNT][SAMPLE_SIZE_16];
	int curr_out, prev_out = BUF_COUNT - 1, audio_buf_counter = 0, num_consecutive_audio_stop = 0;

	audio.update_volume(g_volume, g_mute);
	volatile int num_sleeps = 0;
	volatile int loaded_samples;

	while (g_running) {
		curr_out = (g_curr_in - 1 + BUF_COUNT) % BUF_COUNT;

		if (curr_out != prev_out) {
			loaded_samples = g_samples.size();
			if(loaded_samples >= AUDIO_LATENCY) {
				g_samples.pop();
			}
			if (g_read[curr_out] > FRAME_SIZE_RGB) {
				int n_samples = (g_read[curr_out] - FRAME_SIZE_RGB) / 2;
				if(n_samples > SAMPLE_SIZE_16) {
					n_samples = SAMPLE_SIZE_16;
				}
				map(&g_in_buf[curr_out][FRAME_SIZE_RGB], out_buf[audio_buf_counter], n_samples);
				g_samples.emplace(out_buf[audio_buf_counter], n_samples);
				if(++audio_buf_counter == AUDIO_BUF_COUNT) {
					audio_buf_counter = 0;
				}
			}
			num_sleeps = 0;
			prev_out = curr_out;
		}
		
		loaded_samples = g_samples.size();
		if (audio.getStatus() != sf::SoundStream::Playing) {
			if (loaded_samples > 0) {
				num_consecutive_audio_stop++;
				if (num_consecutive_audio_stop > AUDIO_FAILURE_THRESHOLD) {
					audio.stop();
					audio.~Audio();
					::new(&audio) Audio();
				}
				audio.play();
			}
		}
		else {
			audio.update_volume(g_volume, g_mute);
			num_consecutive_audio_stop = 0;
		}

		if(num_sleeps < AUDIO_NUM_CHECKS) {
			sf::sleep(sf::milliseconds(1000/AUDIO_CHECKS_PER_SECOND));
			++num_sleeps;
		}
	}

	audio.stop();
}

void render() {
	UCHAR out_buf[FRAME_SIZE_RGBA];
	int curr_out, prev_out = 0;

	g_in_tex.create(CAP_WIDTH, CAP_HEIGHT);

	Screen top_screen(&gp_top_blur, &gp_top_crop, &gp_top_rotation, &gp_top_scale);
	Screen bot_screen(&gp_bot_blur, &gp_bot_crop, &gp_bot_rotation, &gp_bot_scale);
	Screen joint_screen(&gp_joint_blur, &gp_joint_crop, &gp_joint_rotation, &gp_joint_scale);

	if (!g_skip_io) {
		load(g_conf_dir + "/", std::string(NAME) + ".conf");
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

		sf::sleep(sf::milliseconds(0));
	}

	thread.join();

	top_screen.m_win.close();
	bot_screen.m_win.close();
	joint_screen.m_win.close();

	if (!g_skip_io) {
		save(g_conf_dir + "/", std::string(NAME) + ".conf");
	}
}

int main(int argc, char **argv) {
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--safe") == 0) {
			g_skip_io = true;
			continue;
		}

		if (strcmp(argv[i], "--vsync") == 0) {
			g_vsync = true;
			continue;
		}

		printf("[%s] Invalid argument \"%s\".\n", NAME, argv[i]);
	}

	g_connected = connect(true);
	#ifdef CLOSE_ON_FAIL
	if(!g_connected)
		return -1;
	#endif
	std::thread thread(capture);

	render();
	thread.join();

	return 0;
}

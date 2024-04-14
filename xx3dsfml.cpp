/*
 * This software is provided as is, without any warranty, express or implied.
 * This software is licensed under a Creative Commons (CC BY-NC-SA) license.
 * This software is authored by Chris Malnick (2023, 2024).
 */

#include <libftd3xx/ftd3xx.h>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
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

#define FRAMERATE_LIMIT 60

#define SAMPLE_LIMIT 3
#define DROP_LIMIT 3

#define TRANSFER_ABORT -1

const std::string CONF_DIR = std::string(std::getenv("HOME")) + "/.config/" + std::string(NAME) + "/";

bool g_running = true;
bool g_finished = false;

bool g_safe_mode = false;

class Capture {
public:
	static inline UCHAR buf[BUF_COUNT][BUF_SIZE];
	static inline ULONG read[BUF_COUNT];

	static inline bool starting = true;

	static inline bool connected = false;
	static inline bool disconnecting = false;

	static inline bool auto_connect = false;

	static inline bool connect() {
		if (Capture::connected) {
			return true;
		}

		if (FT_Create(const_cast<char*>(PRODUCT_1), FT_OPEN_BY_DESCRIPTION, &Capture::handle) && FT_Create(const_cast<char*>(PRODUCT_2), FT_OPEN_BY_DESCRIPTION, &Capture::handle)) {
			printf("[%s] Create failed.\n", NAME);
			return false;
		}

		UCHAR buf[4] = {0x40, 0x80, 0x00, 0x00};
		ULONG written = 0;

		if (FT_WritePipe(Capture::handle, BULK_OUT, buf, 4, &written, 0)) {
			printf("[%s] Write failed.\n", NAME);
			return false;
		}

		buf[1] = 0x00;

		if (FT_WritePipe(Capture::handle, BULK_OUT, buf, 4, &written, 0)) {
			printf("[%s] Write failed.\n", NAME);
			return false;
		}

		if (FT_SetStreamPipe(Capture::handle, false, false, BULK_IN, BUF_SIZE)) {
			printf("[%s] Stream failed.\n", NAME);
			return false;
		}

		for (int i = 0; i < BUF_COUNT; ++i) {
			if (FT_InitializeOverlapped(Capture::handle, &Capture::overlap[i])) {
				printf("[%s] Initialize failed.\n", NAME);
				return false;
			}
		}

		for (int i = 0; i < BUF_COUNT; ++i) {
			if (FT_ReadPipeAsync(Capture::handle, FIFO_CHANNEL, Capture::buf[i], BUF_SIZE, &Capture::read[i], &Capture::overlap[i]) != FT_IO_PENDING) {
				printf("[%s] Read failed.\n", NAME);
				return false;
			}
		}

		return true;
	}

	static inline void stream(std::promise<int> *p_audio_promise, std::promise<int> *p_video_promise, bool *p_audio_waiting, bool *p_video_waiting) {
		while (g_running) {
			if (!Capture::connected) {
				if (Capture::auto_connect) {
					if (!(Capture::connected = Capture::connect())) {
						sf::sleep(sf::milliseconds(5000));
					}
				}

				else {
					sf::sleep(sf::milliseconds(5));
				}

				continue;
			}

			if (Capture::disconnecting || !Capture::transfer()) {
				Capture::disconnecting = Capture::connected = Capture::disconnect();
				Capture::signal(p_video_promise, p_video_waiting, TRANSFER_ABORT);

				Capture::starting = true;
				Capture::index = 0;

				continue;
			}

			Capture::signal(p_audio_promise, p_audio_waiting, Capture::index);
			Capture::signal(p_video_promise, p_video_waiting, Capture::index);

			Capture::index = (Capture::index + 1) % BUF_COUNT;

			if (Capture::starting) {
				Capture::starting = Capture::index;
			}
		}

		Capture::disconnecting = Capture::connected = Capture::disconnect();

		while (!g_finished) {
			Capture::signal(p_audio_promise, p_audio_waiting, TRANSFER_ABORT);
			Capture::signal(p_video_promise, p_video_waiting, TRANSFER_ABORT);

			sf::sleep(sf::milliseconds(5));
		}
	}

private:
	static inline FT_HANDLE handle;
	static inline OVERLAPPED overlap[BUF_COUNT];

	static inline int index = 0;

	static inline bool disconnect() {
		if (!Capture::connected) {
			return false;
		}

		for (int i = 0; i < BUF_COUNT; ++i) {
			if (FT_ReleaseOverlapped(Capture::handle, &Capture::overlap[i])) {
				printf("[%s] Release failed.\n", NAME);
			}
		}

		if (FT_Close(Capture::handle)) {
			printf("[%s] Close failed.\n", NAME);
		}

		return false;
	}

	static inline bool transfer() {
		if (FT_GetOverlappedResult(Capture::handle, &Capture::overlap[Capture::index], &Capture::read[Capture::index], true) == FT_IO_INCOMPLETE && FT_AbortPipe(Capture::handle, BULK_IN)) {
			printf("[%s] Abort failed.\n", NAME);
			return false;
		}

		if (FT_ReadPipeAsync(Capture::handle, FIFO_CHANNEL, Capture::buf[Capture::index], BUF_SIZE, &Capture::read[Capture::index], &Capture::overlap[Capture::index]) != FT_IO_PENDING) {
			printf("[%s] Read failed.\n", NAME);
			return false;
		}

		return true;
	}

	static inline void signal(std::promise<int> *p_promise, bool *p_waiting, int value) {
		if (*p_waiting) {
			*p_waiting = false;
			p_promise->set_value(value);
		}
	}
};

class Audio : public sf::SoundStream {
public:
	static inline Audio *p_audio;

	static inline int volume = 50;
	static inline bool mute = false;

	static inline std::promise<int> promise;
	static inline bool waiting = false;

	Audio() {
		this->initialize(AUDIO_CHANNELS, SAMPLE_RATE);
		this->setVolume(0);

		this->play();
	}

	static inline void adjust() {
		Audio::p_audio->setVolume(Audio::starting || Audio::mute ? 0 : Audio::volume);
	}

	static inline void playback() {
		while (g_running) {
			Audio::promise = std::promise<int>();
			Audio::waiting = true;

			int ready = Audio::promise.get_future().get();

			if (ready == TRANSFER_ABORT) {
				continue;
			}

			if (Capture::starting) {
				Audio::reset();
				continue;
			}

			if (!Audio::load(&Capture::buf[ready][FRAME_SIZE_RGB], &Capture::read[ready])) {
				continue;
			}

			Audio::index = (Audio::index + 1) % BUF_COUNT;

			if (Audio::starting) {
				Audio::starting = Audio::index;
				Audio::adjust();
			}

			Audio::unblock();
		}

		Audio::unblock();
		delete Audio::p_audio;
	}

private:
	struct Sample {
		Sample(sf::Int16 *bytes, std::size_t size) : bytes(bytes), size(size) {}

		sf::Int16 *bytes;
		std::size_t size;
	};

	static inline sf::Int16 buf[BUF_COUNT][SAMPLE_SIZE_16];
	static inline std::queue<Audio::Sample> samples;

	static inline bool starting = true;

	static inline int index = 0;
	static inline int drops = 0;

	static inline std::promise<void> barrier;
	static inline bool blocked = false;

	static inline void reset() {
		Audio::unblock();
		delete Audio::p_audio;

		Audio::samples = {};
		Audio::p_audio = new Audio();

		Audio::starting = true;

		Audio::index = 0;
		Audio::drops = 0;
	}

	static inline bool load(UCHAR *p_buf, ULONG *p_read) {
		if (*p_read <= FRAME_SIZE_RGB) {
			return false;
		}

		if (Audio::samples.size() > SAMPLE_LIMIT) {
			if (++Audio::drops > DROP_LIMIT) {
				Audio::reset();
			}

			else {
				return false;
			}
		}

		Audio::drops = 0;

		Audio::map(p_buf, Audio::buf[Audio::index]);
		Audio::samples.emplace(Audio::buf[Audio::index], (*p_read - FRAME_SIZE_RGB) / 2);

		return true;
	}

	static inline void map(UCHAR *p_in, sf::Int16 *p_out) {
		for (int i = 0; i < SAMPLE_SIZE_16; ++i) {
			p_out[i] = p_in[i * 2 + 1] << 8 | p_in[i * 2];
		}
	}

	static inline void unblock() {
		if (Audio::blocked) {
			Audio::blocked = false;
			Audio::barrier.set_value();
		}
	}

	bool onGetData(sf::SoundStream::Chunk &data) override {
		if (Audio::samples.empty()) {
			Audio::barrier = std::promise<void>();
			Audio::blocked = true;

			Audio::barrier.get_future().wait();

			if (Audio::samples.empty()) {
				return false;
			}
		}

		data.samples = Audio::samples.front().bytes;
		data.sampleCount = Audio::samples.front().size;

		Audio::samples.pop();

		return true;
	}

	void onSeek(sf::Time timeOffset) override {}
};

class Video {
public:
	class Screen {
	public:
		enum Type { TOP, BOT, JOINT, COUNT };
		enum Crop { DEFAULT_3DS, SCALED_DS, NATIVE_DS, END };

		sf::RectangleShape m_in_rect;
		sf::RenderWindow m_win;

		bool m_blur = false;
		Crop m_crop = Video::Screen::Crop::DEFAULT_3DS;
		int m_rotation = 0;
		double m_scale = 1.0;

		Screen() {}

		std::string key() {
			switch (this->m_type) {
			case Video::Screen::Type::TOP:
				return "top";

			case Video::Screen::Type::BOT:
				return "bot";

			case Video::Screen::Type::JOINT:
				return "joint";

			default:
				return "";
			}
		}

		void build(Video::Screen::Type type, int u, int width, bool visible) {
			this->m_type = type;

			this->resize(TOP_WIDTH_3DS, this->height(HEIGHT_3DS));

			this->m_in_rect.setTexture(&Video::in_tex);
			this->m_in_rect.setTextureRect(sf::IntRect(0, u, this->m_height, width));

			this->m_in_rect.setSize(sf::Vector2f(this->m_height, width));
			this->m_in_rect.setOrigin(this->m_height / 2, width / 2);

			this->m_in_rect.setRotation(-90);
			this->m_in_rect.setPosition(width / 2, this->m_height / 2);

			this->m_out_tex.create(width, this->m_height);

			this->m_out_rect.setSize(sf::Vector2f(width, this->m_height));
			this->m_out_rect.setTexture(&this->m_out_tex.getTexture());

			this->m_view.reset(sf::FloatRect(0, 0, width, this->m_height));

			if (visible) {
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
			case Video::Screen::Type::TOP:
				if (Video::split && this->m_crop == Video::Screen::Crop::NATIVE_DS) {
					this->m_in_rect.move(0, -DELTA_HEIGHT_DS / 2);
				}

				return;

			case Video::Screen::Type::BOT:
				if (Video::split) {
					if (this->m_crop == Video::Screen::Crop::NATIVE_DS) {
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
					case sf::Keyboard::Dash:
						this->m_scale = this->m_scale > 1.5 ? static_cast<int>(this->m_scale / 0.5) * 0.5 - 0.5 : 1.0;
						break;

					case sf::Keyboard::Equal:
						this->m_scale = this->m_scale < 4.0 ? static_cast<int>(this->m_scale / 0.5) * 0.5 + 0.5 : 4.5;
						break;

					case sf::Keyboard::LBracket:
						std::swap(this->m_width, this->m_height);

						this->m_rotation = ((this->m_rotation / 90 * 90 + 90) % 360 + 360) % 360;
						this->rotate();

						break;

					case sf::Keyboard::RBracket:
						std::swap(this->m_width, this->m_height);

						this->m_rotation = ((this->m_rotation / 90 * 90 - 90) % 360 + 360) % 360;
						this->rotate();

						break;

					case sf::Keyboard::Quote:
						this->m_crop = static_cast<Crop>(((this->m_crop + 1) % Video::Screen::Crop::END + Video::Screen::Crop::END) % Video::Screen::Crop::END);

						this->crop();
						this->move();

						break;

					case sf::Keyboard::SemiColon:
						this->m_crop = static_cast<Crop>(((this->m_crop - 1) % Video::Screen::Crop::END + Video::Screen::Crop::END) % Video::Screen::Crop::END);

						this->crop();
						this->move();

						break;

					case sf::Keyboard::Comma:
						Audio::volume = Audio::volume > 5 ? Audio::volume / 5 * 5 - 5 : 0;
						Audio::adjust();

						break;

					case sf::Keyboard::Period:
						Audio::volume = Audio::volume < 95 ? Audio::volume / 5 * 5 + 5 : 100;
						Audio::adjust();

						break;
					}

					break;

				case sf::Event::KeyReleased:
					switch (this->m_event.key.code) {
					case sf::Keyboard::C:
						if (!Capture::auto_connect) {
							Capture::connected ? Capture::disconnecting = true : Capture::connected = Capture::connect();
						}

						break;

					case sf::Keyboard::S:
						Video::split ^= true;
						Video::swap();

						break;

					case sf::Keyboard::B:
						this->m_out_tex.setSmooth(this->m_blur ^= true);
						break;

					case sf::Keyboard::M:
						Audio::mute ^= true;
						Audio::adjust();

						break;

					case sf::Keyboard::F1:
					case sf::Keyboard::F2:
					case sf::Keyboard::F3:
					case sf::Keyboard::F4:
					case sf::Keyboard::F5:
					case sf::Keyboard::F6:
					case sf::Keyboard::F7:
					case sf::Keyboard::F8:
					case sf::Keyboard::F9:
					case sf::Keyboard::F10:
					case sf::Keyboard::F11:
					case sf::Keyboard::F12:
						if (!g_safe_mode) {
							if (this->m_event.key.control) {
								Video::p_save(CONF_DIR + "presets/", "layout" + std::to_string(this->m_event.key.code - sf::Keyboard::F1 + 1) + ".conf");
							}

							else {
								Video::p_load(CONF_DIR + "presets/", "layout" + std::to_string(this->m_event.key.code - sf::Keyboard::F1 + 1) + ".conf");

								Audio::adjust();
								Video::init();
							}
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
		sf::RenderTexture m_out_tex;
		sf::RectangleShape m_out_rect;

		sf::View m_view;
		sf::Event m_event;

		Screen::Type m_type;

		int m_width = 0;
		int m_height = 0;

		bool horizontal() {
			return this->m_rotation / 10 % 2;
		}

		int height(int height) {
			return height * (this->m_type == Video::Screen::Type::JOINT ? 2 : 1);
		}

		std::string title() {
			switch (this->m_type) {
			case Video::Screen::Type::TOP:
				return std::string(NAME) + "-top";

			case Video::Screen::Type::BOT:
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

			Video::vsync ? this->m_win.setVerticalSyncEnabled(true) : this->m_win.setFramerateLimit(FRAMERATE_LIMIT);
		}

		void rotate() {
			this->m_view.setRotation(this->m_rotation);

			this->m_view.setSize(this->m_width, this->m_height);
			this->m_win.setView(this->m_view);
		}

		void crop() {
			switch (this->m_crop) {
			case Video::Screen::Crop::DEFAULT_3DS:
				this->horizontal() ? this->resize(this->height(HEIGHT_3DS), TOP_WIDTH_3DS) : this->resize(TOP_WIDTH_3DS, this->height(HEIGHT_3DS));
				break;

			case Video::Screen::Crop::SCALED_DS:
				this->horizontal() ? this->resize(this->height(HEIGHT_3DS), BOT_WIDTH_3DS) : this->resize(BOT_WIDTH_3DS, this->height(HEIGHT_3DS));
				break;

			case Video::Screen::Crop::NATIVE_DS:
				this->horizontal() ? this->resize(this->height(HEIGHT_DS), WIDTH_DS) : this->resize(WIDTH_DS, this->height(HEIGHT_DS));
				break;
			}

			this->m_view.setSize(this->m_width, this->m_height);
			this->m_win.setView(this->m_view);
		}
	};

	static inline Screen screens[Video::Screen::Type::COUNT];
	static inline sf::Texture in_tex;

	static inline bool split = false;
	static inline bool vsync = false;

	static inline std::promise<int> promise;
	static inline bool waiting = false;

	static inline void (*p_load) (std::string path, std::string name);
	static inline void (*p_save) (std::string path, std::string name);

	static inline Screen *screen(std::string key) {
		if (key == "top") {
			return &Video::screens[Video::Screen::Type::TOP];
		}

		if (key == "bot") {
			return &Video::screens[Video::Screen::Type::BOT];
		}

		if (key == "joint") {
			return &Video::screens[Video::Screen::Type::JOINT];
		}

		return nullptr;
	}

	static inline void init() {
		Video::screens[Video::Screen::Type::TOP].reset();
		Video::screens[Video::Screen::Type::BOT].reset();
		Video::screens[Video::Screen::Type::JOINT].reset();

		if (!(Video::screens[Video::Screen::Type::JOINT].m_win.isOpen() ^ Video::split)) {
			Video::screens[Video::Screen::Type::TOP].toggle();
			Video::screens[Video::Screen::Type::BOT].toggle();
			Video::screens[Video::Screen::Type::JOINT].toggle();
		}
	}

	static inline void blank() {
		memset(Video::buf, 0x00, FRAME_SIZE_RGBA);
		Video::in_tex.update(Video::buf, CAP_WIDTH, CAP_HEIGHT, 0, 0);

		Video::draw();
	}

	static inline void render() {
		while (g_running) {
			Video::poll();

			if (!Capture::connected) {
				Video::blank();
				sf::sleep(sf::milliseconds(5));

				continue;
			}

			Video::promise = std::promise<int>();
			Video::waiting = true;

			int ready = Video::promise.get_future().get();

			if (ready == TRANSFER_ABORT) {
				continue;
			}

			if (Capture::starting) {
				Video::blank();
				continue;
			}

			if (!Video::load(Capture::buf[ready], &Capture::read[ready])) {
				continue;
			}

			Video::draw();
		}
	}

private:
	static inline UCHAR buf[FRAME_SIZE_RGBA];

	static inline void swap() {
		Video::screens[Video::Screen::Type::TOP].toggle();
		Video::screens[Video::Screen::Type::BOT].toggle();
		Video::screens[Video::Screen::Type::JOINT].toggle();

		Video::screens[Video::Screen::Type::TOP].move();
		Video::screens[Video::Screen::Type::BOT].move();
	}

	static inline void poll() {
		Video::screens[Video::Screen::Type::TOP].poll();
		Video::screens[Video::Screen::Type::BOT].poll();
		Video::screens[Video::Screen::Type::JOINT].poll();
	}

	static inline bool load(UCHAR *p_buf, ULONG *p_read) {
		if (*p_read < FRAME_SIZE_RGB) {
			return false;
		}

		Video::map(p_buf, Video::buf);
		Video::in_tex.update(Video::buf, CAP_WIDTH, CAP_HEIGHT, 0, 0);

		return true;
	}

	static inline void map(UCHAR *p_in, UCHAR *p_out) {
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

	static inline void draw() {
		if (Video::split) {
			Video::screens[Video::Screen::Type::TOP].draw();
			Video::screens[Video::Screen::Type::BOT].draw();
		}

		else {
			Video::screens[Video::Screen::Type::JOINT].draw(&Video::screens[Video::Screen::Type::TOP].m_in_rect, &Video::screens[Video::Screen::Type::BOT].m_in_rect);
		}
	}
};

void load(std::string path, std::string name) {
	std::ifstream file(path + name);

	if (!file.good()) {
		printf("[%s] File \"%s\" load failed.\n", NAME, name.c_str());
		return;
	}

	std::string line;

	while (std::getline(file, line)) {
		std::istringstream kvp(line);
		std::string key;

		Video::Screen *p_screen;

		if (std::getline(kvp, key, '_')) {
			p_screen = Video::screen(key);
		}

		if (!p_screen) {
			kvp.str(line);
			kvp.clear();
		}

		if (std::getline(kvp, key, '=')) {
			std::string value;

			if (std::getline(kvp, value)) {
				if (key == "volume") {
					Audio::volume = std::clamp(std::stoi(value) / 5 * 5, 0, 100);
					continue;
				}

				if (key == "mute") {
					Audio::mute = std::stoi(value);
					continue;
				}

				if (key == "split") {
					Video::split = std::stoi(value);
					continue;
				}

				if (key == "blur") {
					p_screen->m_blur = std::stoi(value);
					continue;
				}

				if (key == "crop") {
					p_screen->m_crop = static_cast<Video::Screen::Crop>((std::stoi(value) % Video::Screen::Crop::END + Video::Screen::Crop::END) % Video::Screen::Crop::END);
					continue;
				}

				if (key == "rotation") {
					p_screen->m_rotation = (std::stoi(value) / 90 * 90 % 360 + 360) % 360;
					continue;
				}

				if (key == "scale") {
					p_screen->m_scale = std::clamp(static_cast<int>(std::stod(value) / 0.5) * 0.5, 1.0, 4.5);
					continue;
				}
			}
		}
	}
}

void save(std::string path, std::string name) {
	std::filesystem::create_directories(path);
	std::ofstream file(path + name);

	if (!file.good()) {
		printf("[%s] File \"%s\" save failed.\n", NAME, name.c_str());
		return;
	}

	file << "volume=" << Audio::volume << std::endl;
	file << "mute=" << Audio::mute << std::endl;
	file << "split=" << Video::split << std::endl;

	for (int i = 0; i < Video::Screen::Type::COUNT; ++i) {
		std::string key = Video::screens[i].key();

		file << key << "_blur=" << Video::screens[i].m_blur << std::endl;
		file << key << "_crop=" << Video::screens[i].m_crop << std::endl;
		file << key << "_rotation=" << Video::screens[i].m_rotation << std::endl;
		file << key << "_scale=" << Video::screens[i].m_scale << (Video::screens[i].m_scale - static_cast<int>(Video::screens[i].m_scale) ? "" : ".0") << std::endl;
	}
}

int main(int argc, char **argv) {
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--auto") == 0) {
			Capture::auto_connect = true;
			continue;
		}

		if (strcmp(argv[i], "--safe") == 0) {
			g_safe_mode = true;
			continue;
		}

		if (strcmp(argv[i], "--vsync") == 0) {
			Video::vsync = true;
			continue;
		}

		printf("[%s] Invalid argument \"%s\".\n", NAME, argv[i]);
	}

	if (!g_safe_mode) {
		load(CONF_DIR, std::string(NAME) + ".conf");
	}

	Capture::connected = Capture::connect();
	Audio::p_audio = new Audio();

	Video::p_load = &load;
	Video::p_save = &save;

	Video::screens[Video::Screen::Type::TOP].build(Video::Screen::Type::TOP, 0, TOP_WIDTH_3DS, Video::split);
	Video::screens[Video::Screen::Type::BOT].build(Video::Screen::Type::BOT, TOP_WIDTH_3DS, BOT_WIDTH_3DS, Video::split);
	Video::screens[Video::Screen::Type::JOINT].build(Video::Screen::Type::JOINT, 0, TOP_WIDTH_3DS, !Video::split);

	Video::in_tex.create(CAP_WIDTH, CAP_HEIGHT);

	Video::init();
	Video::blank();

	std::thread capture = std::thread(Capture::stream, &Audio::promise, &Video::promise, &Audio::waiting, &Video::waiting);
	std::thread audio = std::thread(Audio::playback);

	Video::render();
	audio.join();

	g_finished = true;
	capture.join();

	Video::screens[Video::Screen::Type::TOP].m_win.close();
	Video::screens[Video::Screen::Type::BOT].m_win.close();
	Video::screens[Video::Screen::Type::JOINT].m_win.close();

	if (!g_safe_mode) {
		save(CONF_DIR, std::string(NAME) + ".conf");
	}

	return 0;
}

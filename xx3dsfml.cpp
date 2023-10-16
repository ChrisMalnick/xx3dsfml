/*
 * This software is provided as is, without any warranty, express or implied.
 * Copyright 2023 Chris Malnick. All rights reserved.
 */

#include <libftd3xx/ftd3xx.h>
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <thread>
#include <unistd.h> 
#include <iostream>
#include <queue>
#include <vector>

#define WINDOWS 2

#define NAME "xx3dsfml"
#define NUM_PRODUCTS 2
#define PRODUCTS (const char*[]){"N3DSXL", "N3DSXL.2"}

#define BULK_OUT 0x02
#define BULK_IN 0x82
#define TBULK_IN 0x86

#define FIFO_CHANNEL 0

#define CAP_WIDTH 240
#define CAP_HEIGHT 720

#define CAP_RES (CAP_WIDTH * CAP_HEIGHT)

#define RGB_FRAME_SIZE (CAP_RES * 3)
#define RGBA_FRAME_SIZE (CAP_RES * 4)

#define AUDIO_BUF_SIZE 2192
#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLE_RATE 32728

#define BUF_COUNT 8
#define BUF_SIZE (RGB_FRAME_SIZE + AUDIO_BUF_SIZE)

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
bool already_ask_audio = false;
bool disconnect_and_connect = false;

int curr_buf = 0;
UCHAR in_buf[BUF_COUNT][BUF_SIZE];
ULONG len_buf[BUF_COUNT];

typedef std::vector<sf::Int16> IntVector;
class N3DSAudio : public sf::SoundStream
{
public:
	N3DSAudio(){
		initialize(AUDIO_CHANNELS, AUDIO_SAMPLE_RATE);
		setProcessingInterval(sf::milliseconds(0));
	} 

	void queue(IntVector samples, size_t seed){
		m_bmux.lock();
		if(seed != last_seed && m_buff.size() <= BUF_COUNT * 10){
			m_buff.push(samples);
			last_seed = seed;
		}
		m_bmux.unlock();
	}
private:
	sf::Mutex m_bmux;
	std::queue<IntVector> m_buff;
	size_t last_seed;

	size_t m_sampleRate;

	virtual bool onGetData(Chunk& data){
		m_bmux.lock();
		if(m_buff.size()<1)
		{
			m_bmux.unlock();
			return true;
		}

		IntVector bu = m_buff.front();
		m_buff.pop();

		m_bmux.unlock();


		size_t bfs=bu.size();
		sf::Int16* bf=new sf::Int16[bfs]; 

		for(size_t i=0;i<bfs;++i)
			bf[i]=bu[i];

		
		data.samples   = bf;
		data.sampleCount = bfs;

		return true;
	}

	virtual void onSeek(sf::Time timeOffset){
		// no op
	}
};

bool handle_open(){
	for (int i=0; i < NUM_PRODUCTS; i++) {
		char* product = const_cast<char*>(PRODUCTS[i]);
		if (FT_Create(product, FT_OPEN_BY_DESCRIPTION, &handle) == FT_OK) {
			printf("[%s] Create for %s successed.\n", NAME, PRODUCTS[i]);
			return true;
		}
	}
	return false;
}

bool ask_for_audio(){
	if(already_ask_audio){
		return true;
	}

	UCHAR buf[4] = {0x40, 0x80, 0x00, 0x00};
	ULONG written = 0;

	if (FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0)) {
		FT_AbortPipe(handle, BULK_IN);
		printf("[%s] Write 1 failed.\n", NAME);
		return false;
	}
	
	if (FT_AbortPipe(handle, BULK_OUT)) {
		printf("[%s] Abort failed.\n", NAME);
		return false;
	}
	
	UCHAR buf2[4] = {0x05, 0x98, 0x00, 0x00};
	ULONG returned = 0;

	if (FT_WritePipe(handle, BULK_OUT, buf2, 4, &returned, 0)) {
		FT_AbortPipe(handle, BULK_IN);
		printf("[%s] Write 2 failed.\n", NAME);
		return false;
	}

	if (FT_ReadPipe(handle, BULK_IN, buf2, 4, &returned, 0)) {
		FT_AbortPipe(handle, BULK_IN);
		printf("[%s] Read failed.\n", NAME);
		return false;
	}

	already_ask_audio = true;

	return true;
}

bool initialize(){
	UCHAR buf[4] = {0x40, 0x00, 0x00, 0x00};
	ULONG written = 0;

	if (FT_WritePipe(handle, BULK_OUT, buf, 4, &written, 0)) {
		FT_AbortPipe(handle, BULK_IN);
		printf("[%s] Write failed.\n", NAME);
		return false;
	}

	if (FT_SetStreamPipe(handle, false, false, BULK_IN, BUF_SIZE)) {
		printf("[%s] Stream failed.\n", NAME);
		return false;
	}

	return true;
}

bool open() {
	disconnect_and_connect = false;

	if (connected || handle != NULL) {
		return false;
	}

	if (!handle_open()) {
		printf("[%s] Create failed.\n", NAME);
		return false;
	}

	if(FT_SetPipeTimeout(handle, BULK_OUT, 200)){
		printf("[%s] Timeout failed.\n", NAME);
		return false;
	}

	if(FT_SetPipeTimeout(handle, BULK_IN, 80)){
		printf("[%s] Timeout failed.\n", NAME);
		return false;
	}

	if(FT_ClearStreamPipe(handle, true, true, 0)){
		printf("[%s] Clear failed.\n", NAME);
	}

	if (!ask_for_audio()) {
		printf("[%s] Ask for audio failed.\n", NAME);

		if (FT_Close(handle)) {
			printf("[%s] Close failed.\n", NAME);
		}

		disconnect_and_connect = true;
		handle = NULL;

		return false;
	}
	
	if(!initialize()){
		printf("[%s] Initialize failed.\n", NAME);
		

		return false;
	}

	return true;
}

void capture() {
	ULONG read[BUF_COUNT];
	OVERLAPPED overlap[BUF_COUNT];

start:
	while (!connected) {
		if(disconnect_and_connect){
			printf("[%s] please disconnect and connect cable, then press 1.\n", NAME);
		}else{
			printf("[%s] waiting.\n", NAME);
		}

		if (!running) {
			return;
		}
		sleep(1);
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

		len_buf[curr_buf] = read[curr_buf];

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

	if(FT_ClearStreamPipe(handle, false, false, BULK_IN)){
		printf("[%s] Clear failed.\n", NAME);
	}

	if (FT_Close(handle)) {
		printf("[%s] Close failed.\n", NAME);
	}

	printf("[%s] Connection closed.\n", NAME);
	connected = false;
	sleep(5); // need to find a better way to avoid segmantation fault when multiple on/off
	handle = NULL;

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

void audio(UCHAR *p_in, ULONG end, N3DSAudio *soundStream) {
	IntVector sample;
	size_t seed = RGB_FRAME_SIZE - end;
	for (size_t i=RGB_FRAME_SIZE;i<end; i=i+2) {
		sf::Int16 sound = (p_in[i+1] << 8) | p_in[i];
		sample.push_back(sound);
		seed ^= sound + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
	
	if(sample.size()> 0){
		soundStream->queue(sample, seed);
		
		if(soundStream->getStatus() != sf::SoundSource::Playing)
			soundStream->play();
	}
}

void render() {
	std::thread thread(capture);

	int win_width = TOP_WIDTH;
	int win_height = TOP_HEIGHT + BOT_HEIGHT;

	int wint_width = TOP_WIDTH;
	int wint_height = TOP_HEIGHT;

	int winb_width = BOT_WIDTH;
	int winb_height = BOT_HEIGHT;

	int scale = 1;

	UCHAR out_buf[RGBA_FRAME_SIZE];
	sf::RenderWindow* win[2];
	if(WINDOWS == 2){
		win[0] = new sf::RenderWindow(sf::VideoMode(wint_width, wint_height), NAME);
		win[1] = new sf::RenderWindow(sf::VideoMode(winb_width, winb_height), NAME);
	}else{
		win[0] = new sf::RenderWindow(sf::VideoMode(win_width, win_height), NAME);
	}

	sf::RectangleShape top_rect(sf::Vector2f(wint_height, wint_width));
	sf::RectangleShape bot_rect(sf::Vector2f(winb_height, winb_width));

	sf::RectangleShape out_rect(sf::Vector2f(win_width, win_height));

	sf::Texture in_tex;
	sf::RenderTexture out_tex;

	sf::Event event;	

	N3DSAudio *audioStream = new N3DSAudio();

	win[0]->setFramerateLimit(FRAMERATE + FRAMERATE / 2);
	win[0]->setKeyRepeatEnabled(false);
	
	if(WINDOWS == 2){
		win[1]->setFramerateLimit(FRAMERATE + FRAMERATE / 2);
		win[1]->setKeyRepeatEnabled(false);

		sf::View viewt(sf::FloatRect(0, 0, wint_width, wint_height));
		win[0]->setView(viewt);

		sf::View viewb(sf::FloatRect(0, 0, winb_width, winb_height));
		win[1]->setView(viewb);
		
		win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
		win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
	}else{
		sf::View view(sf::FloatRect(0, 0, win_width, win_height));
		win[0]->setView(view);
		win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
	}

	top_rect.setOrigin(wint_height / 2, wint_width / 2);
	top_rect.setPosition(wint_width / 2, wint_height / 2);
	top_rect.setRotation(-90);

	bot_rect.setOrigin(winb_height / 2, winb_width / 2);
	if(WINDOWS == 2){
		bot_rect.setPosition(winb_width / 2, winb_height / 2);
	}else{
		bot_rect.setPosition(win_width / 2, TOP_HEIGHT + BOT_HEIGHT - (winb_height / 2));
	}
	bot_rect.setRotation(-90);

	out_rect.setOrigin(win_width / 2, win_height / 2);
	out_rect.setPosition(win_width / 2, win_height / 2);

	in_tex.create(CAP_WIDTH, CAP_HEIGHT);
	out_tex.create(win_width, win_height);

	top_rect.setTexture(&in_tex);
	top_rect.setTextureRect(sf::IntRect(0, 0, TOP_HEIGHT, TOP_WIDTH));

	bot_rect.setTexture(&in_tex);
	bot_rect.setTextureRect(sf::IntRect(0, TOP_WIDTH, BOT_HEIGHT, BOT_WIDTH));

	out_rect.setTexture(&out_tex.getTexture());

	while (win[0]->isOpen()) {
		try {
			while (win[0]->pollEvent(event)) {
				switch (event.type) {
				case sf::Event::Closed:
					win[0]->close();
					if(WINDOWS == 2){
						win[1]->close();
					}
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
						if(WINDOWS == 2){
							win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
							win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
						}else{
							win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
						}
						break;

					case sf::Keyboard::Num4:
						scale += scale == 4 ? 0 : 1;
						if(WINDOWS == 2){
							win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
							win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
						}else{
							win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
						}
						break;

					case sf::Keyboard::Num5:
						if(WINDOWS == 2){
							std::swap(wint_width, wint_height);
							std::swap(winb_width, winb_height);

							top_rect.setSize(sf::Vector2f(wint_height, wint_width));
							top_rect.setOrigin(wint_height / 2, wint_width / 2);
							top_rect.rotate(-90);

							bot_rect.setSize(sf::Vector2f(winb_height, winb_width));
							bot_rect.setOrigin(winb_height / 2, winb_width / 2);
							bot_rect.rotate(-90);

							win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
							win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
						}else{
							std::swap(win_width, win_height);

							out_rect.setSize(sf::Vector2f(win_width, win_height));
							out_rect.setOrigin(win_width / 2, win_height / 2);
							out_rect.rotate(-90);

							win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
						}

						break;

					case sf::Keyboard::Num6:
						if(WINDOWS == 2){
							std::swap(wint_width, wint_height);
							std::swap(winb_width, winb_height);

							top_rect.setSize(sf::Vector2f(wint_height, wint_width));
							top_rect.setOrigin(wint_height / 2, wint_width / 2);
							top_rect.rotate(90);

							bot_rect.setSize(sf::Vector2f(winb_height, winb_width));
							bot_rect.setOrigin(winb_height / 2, winb_width / 2);
							bot_rect.rotate(90);

							win[0]->setSize(sf::Vector2u(wint_width * scale, wint_height * scale));
							win[1]->setSize(sf::Vector2u(winb_width * scale, winb_height * scale));
						}else{
							std::swap(win_width, win_height);

							out_rect.setSize(sf::Vector2f(win_width, win_height));
							out_rect.setOrigin(win_width / 2, win_height / 2);
							out_rect.rotate(90);

							win[0]->setSize(sf::Vector2u(win_width * scale, win_height * scale));
						}

						break;

					default:
						break;
					}

					break;

				default:
					break;
				}
			}

			win[0]->clear();
			if(WINDOWS == 2){
				win[1]->clear();
			}

			if (connected) {
				int idx = (curr_buf == 0 ? BUF_COUNT : curr_buf) - 1;
				map(in_buf[idx], out_buf);
				audio(in_buf[idx], len_buf[idx], audioStream);

				in_tex.update(out_buf, CAP_WIDTH, CAP_HEIGHT, 0, 0);

				out_tex.clear();

				out_tex.draw(top_rect);
				out_tex.draw(bot_rect);

				out_tex.display();

				if(WINDOWS == 2){
					win[0]->draw(top_rect);
					win[1]->draw(bot_rect);
				}else{
					win[0]->draw(out_rect);
				}
			}

			win[0]->display();
			if(WINDOWS == 2){
				win[1]->display();
			}
		}catch(...){
			printf("[%s] Exception.\n", NAME);
		}
	}
	
	running = false;
	thread.join();
}

int main() {
	connected = open();
	render();

	return 0;
}

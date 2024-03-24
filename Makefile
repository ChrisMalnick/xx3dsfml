SYS := ${shell uname -s}
ARC := ${shell uname -m}
TAR := libftd3xx-linux-arm-v6-hf-
VER := 1.0.14
ifeq (${SYS}, Darwin)
	EXT := dylib
	LIB := libftd3xx.${VER}.${EXT}
	UPD := update_dyld_shared_cache
	TAR := d3xx-osx.
else ifeq (${SYS}, Linux)
	EXT := so
	LIB := libftd3xx.${EXT}.$(VER)
	UPD := ldconfig
	ifeq (${ARC}, $(filter aarch% arm%, ${ARC}))
		ifeq (${ARC}, $(filter arm64% %64 armv8% %v8, ${ARC}))
			TAR := libftd3xx-linux-arm-v8-
		else ifeq (${ARC}, $(filter armv7% %v7, ${ARC}))
			TAR := libftd3xx-linux-arm-v7_32-
		endif
	else
		ifeq (${ARC}, $(filter %64, ${ARC}))
			TAR := libftd3xx-linux-x86_64-
		else ifeq (${ARC}, $(filter %32 %86, ${ARC}))
			TAR := libftd3xx-linux-x86_32-
		endif
	endif
endif

xx3dsfml: xx3dsfml.o
	g++ xx3dsfml.o -o xx3dsfml -lftd3xx -lsfml-audio -lsfml-graphics -lsfml-system -lsfml-window

xx3dsfml.o: xx3dsfml.cpp
	g++ -Ofast -c xx3dsfml.cpp -o xx3dsfml.o

clean:
	rm xx3dsfml xx3dsfml.o

install:
	sudo mkdir temp
	sudo curl https://ftdichip.com/wp-content/uploads/2023/06/${TAR}${VER}.tgz -o temp/${TAR}${VER}.tgz
	sudo tar -xzf temp/${TAR}${VER}.tgz --strip-components 1 -C temp
	sudo mkdir -p /usr/local/lib
	sudo cp temp/${LIB} /usr/local/lib
	sudo chmod 755 /usr/local/lib/${LIB}
	sudo ln -sf /usr/local/lib/${LIB} /usr/local/lib/libftd3xx.${EXT}
	sudo ${UPD}
	sudo mkdir -p /usr/local/include/libftd3xx
	sudo cp temp/*.h /usr/local/include/libftd3xx
	sudo chmod 644 /usr/local/include/libftd3xx/*.h
	sudo rm -r temp

uninstall:
	sudo rm /usr/local/lib/libftd3xx.*
	sudo rm -r /usr/local/include/libftd3xx

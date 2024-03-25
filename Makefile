SYS := ${shell uname -s}
ARC := ${shell uname -m}
VER := 1.0.5
TAR := libftd3xx-linux-arm-v6-hf-${VER}.tgz
ifeq (${SYS}, Darwin)
	EXT := dylib
	LIB := libftd3xx.${VER}.${EXT}
	UPD := update_dyld_shared_cache
	TAR := d3xx-osx.${VER}.dmg
	ZIP := 7z x temp/${TAR} -otemp
else ifeq (${SYS}, Linux)
	EXT := so
	LIB := libftd3xx.${EXT}.$(VER)
	UPD := ldconfig
	ifeq (${ARC}, $(filter aarch% arm%, ${ARC}))
		ifeq (${ARC}, $(filter arm64% %64 armv8% %v8, ${ARC}))
			TAR := libftd3xx-linux-arm-v8-${VER}.tgz
		else ifeq (${ARC}, $(filter armv7% %v7, ${ARC}))
			TAR := libftd3xx-linux-arm-v7_32-${VER}.tgz
		endif
	else
		ifeq (${ARC}, $(filter %64, ${ARC}))
			TAR := libftd3xx-linux-x86_64-${VER}.tgz
		else ifeq (${ARC}, $(filter %32 %86, ${ARC}))
			TAR := libftd3xx-linux-x86_32-${VER}.tgz
		endif
	endif
	ZIP := tar -xzf temp/${TAR} --strip-components 1 -C temp
endif

xx3dsfml: xx3dsfml.o
	sudo g++ xx3dsfml.o -o xx3dsfml -lftd3xx -lsfml-audio -lsfml-graphics -lsfml-system -lsfml-window
	sudo rm xx3dsfml.o
	sudo mv xx3dsfml /usr/local/bin
	mkdir -p ~/.config/xx3dsfml/presets

xx3dsfml.o: xx3dsfml.cpp
	sudo g++ -c xx3dsfml.cpp -o xx3dsfml.o

clean:
	sudo rm /usr/local/bin/xx3dsfml
	rm -r ~/.config/xx3dsfml

install:
	sudo mkdir temp
	sudo curl https://ftdichip.com/wp-content/uploads/2023/03/${TAR} -o temp/${TAR}
	sudo ${ZIP}
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

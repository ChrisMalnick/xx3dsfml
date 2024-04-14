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
	${CXX} xx3dsfml.o -o xx3dsfml -pthread -lftd3xx -lsfml-audio -lsfml-graphics -lsfml-system -lsfml-window

xx3dsfml.o: xx3dsfml.cpp
	${CXX} -std=c++17 -c xx3dsfml.cpp -o xx3dsfml.o

clean:
	rm -f xx3dsfml *.o

install:
	mkdir -p temp /usr/local/bin /usr/local/include/libftd3xx /usr/local/lib
	curl https://ftdichip.com/wp-content/uploads/2023/03/${TAR} -o temp/${TAR}
	${ZIP}
	cp temp/*.h /usr/local/include/libftd3xx
	cp temp/${LIB} /usr/local/lib
	rm -rf temp
	chmod 644 /usr/local/include/libftd3xx/*.h
	chmod 755 /usr/local/lib/${LIB}
	ln -sf /usr/local/lib/${LIB} /usr/local/lib/libftd3xx.${EXT}
	${UPD}
	${CXX} -std=c++17 -o /usr/local/bin/xx3dsfml xx3dsfml.cpp -pthread -lftd3xx -lsfml-audio -lsfml-graphics -lsfml-system -lsfml-window

uninstall:
	rm -rf /usr/local/bin/xx3dsfml /usr/local/include/libftd3xx /usr/local/lib/libftd3xx.*

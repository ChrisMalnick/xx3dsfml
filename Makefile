SYS := ${shell uname -s}
ARC := ${shell uname -m}
VER := 1.0.5
TAR := libftd3xx-linux-arm-v6-hf-${VER}.tgz
ifeq (${SYS}, Darwin)
	USR := ${shell logname}
	GRP := admin
	EXT := dylib
	LIB := libftd3xx.${VER}.${EXT}
	UPD := true
	TAR := d3xx-osx.${VER}.dmg
	ZIP := 7z x temp/${TAR} -otemp
	USB := true
else ifeq (${SYS}, Linux)
	USR := root
	GRP := root
	EXT := so
	LIB := libftd3xx.${EXT}.$(VER)
	UPD := ldconfig /usr/local/lib
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
	USB := install -m 755 -o ${USR} -g ${GRP} -d /etc/udev/rules.d && install -m 644 -o ${USR} -g ${GRP} temp/51-ftd3xx.rules /etc/udev/rules.d && udevadm control --reload-rules && udevadm trigger
endif

xx3dsfml: xx3dsfml.o
	${CXX} xx3dsfml.o -o xx3dsfml -pthread -lftd3xx -lsfml-audio -lsfml-graphics -lsfml-system -lsfml-window

xx3dsfml.o: xx3dsfml.cpp
	${CXX} -std=c++17 -c xx3dsfml.cpp -o xx3dsfml.o

clean:
	rm -rf xx3dsfml *.o

ftd3xx:
	curl --create-dirs https://ftdichip.com/wp-content/uploads/2023/03/${TAR} -o temp/${TAR}
	${ZIP}
	${USB}
	install -m 755 -o ${USR} -g ${GRP} -d /usr/local/include/ftd3xx && install -m 644 -o ${USR} -g ${GRP} temp/*.h /usr/local/include/ftd3xx
	install -m 755 -o ${USR} -g ${GRP} -d /usr/local/lib && install -m 755 -o ${USR} -g ${GRP} temp/${LIB} /usr/local/lib && ln -sf /usr/local/lib/${LIB} /usr/local/lib/libftd3xx.${EXT} && ${UPD}
	rm -rf temp

install: ftd3xx xx3dsfml
	install -m 755 -o ${USR} -g ${GRP} -d /usr/local/bin && install -m 755 -o ${USR} -g ${GRP} xx3dsfml /usr/local/bin

uninstall:
	rm -rf /etc/udev/rules.d/51-ftd3xx.rules /usr/local/bin/xx3dsfml /usr/local/include/ftd3xx /usr/local/lib/libftd3xx.*

update:
	curl --create-dirs https://raw.githubusercontent.com/ChrisMalnick/xx3dsfml/main/{LICENSE,Makefile,README.md,xx3dsfml.cpp} -o "#1"

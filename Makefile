xx3dsfml: xx3dsfml.o
	g++ xx3dsfml.o -o xx3dsfml -lftd3xx -lsfml-audio -lsfml-graphics -lsfml-system -lsfml-window

xx3dsfml.o: xx3dsfml.cpp
	g++ -c xx3dsfml.cpp -o xx3dsfml.o

clean:
	rm xx3dsfml xx3dsfml.o

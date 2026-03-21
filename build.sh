#!/bin/bash
g++ -std=c++11 -o pipaint drawingcanvas.cpp touchhandler.cpp main.cpp `sdl2-config --cflags --libs` -lSDL2_image -lSDL2_ttf -levdev

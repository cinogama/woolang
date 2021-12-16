#!/bin/bash
enca -L zh_CN -x utf-8 *.hpp
enca -L zh_CN -x utf-8 *.cpp
enca -L zh_CN -x utf-8 *.h
enca -L zh_CN -x utf-8 *.c
g++ RestorableScene/*.cpp -O0 -std=c++17 -lpthread -ldl -shared -fPIC -o librscene.so -m64
g++ RSceneDriver/*.cpp -O0 -std=c++17 -lrscene -L./ -Wl,-rpath=./ -o rscene -m64
g++ RestorableScene/*.cpp -O0 -std=c++17 -lpthread -ldl -shared -fPIC -o librscene32.so -m32
g++ RSceneDriver/*.cpp -O0 -std=c++17 -lrscene -L./ -Wl,-rpath=./ -o rscene32 -m32

#!/bin/bash
enca -L zh_CN -x utf-8 RestorableScene/*.hpp
enca -L zh_CN -x utf-8 RestorableScene/*.cpp
enca -L zh_CN -x utf-8 RestorableScene/*.h
enca -L zh_CN -x utf-8 RestorableScene/*.c

enca -L zh_CN -x utf-8 RSceneDriver/*.hpp
enca -L zh_CN -x utf-8 RSceneDriver/*.cpp
enca -L zh_CN -x utf-8 RSceneDriver/*.h
enca -L zh_CN -x utf-8 RSceneDriver/*.c

g++ RestorableScene/*.cpp -g -O2 -std=c++17 -lpthread -ldl -shared -fPIC -o librscene.so -m64
g++ RSceneDriver/*.cpp -g -O2 -std=c++17 -lrscene -L./ -Wl,-rpath=./ -o rscene -m64
# g++ RestorableScene/*.cpp -O2 -std=c++17 -lpthread -ldl -shared -fPIC -o librscene32.so -m32
# g++ RSceneDriver/*.cpp -O2 -std=c++17 -lrscene -L./ -Wl,-rpath=./ -o rscene32 -m32

g++ RestorableScene/*.cpp -O0 -std=c++17 -lpthread -ldl -shared -fPIC -o librscene.so
g++ RSceneDriver/*.cpp -O0 -std=c++17 -lrscene -L./ -Wl,-rpath=./ -o rscene

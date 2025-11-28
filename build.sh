set -xe

g++ -o sampler -Wall -Wextra \
    -Isrc src/*.cpp \
    -Ivendor/kissfft vendor/kissfft/*.c \
    -lglfw -lGLEW -lGL \
    -lportaudio -lasound -lm -lpthread \
    -lsndfile
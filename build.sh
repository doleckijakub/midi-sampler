set -xe

g++ -o sampler -Wall -Wextra -Isrc src/*.cpp \
    -lglfw -lGLEW -lGL \
    -lportaudio -lasound -lm -lpthread \
    -lsndfile
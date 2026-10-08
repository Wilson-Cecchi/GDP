#!/bin/sh
set -eu
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends ca-certificates cmake g++ ninja-build pkg-config \
    libgl-dev libopengl-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
    libxi-dev libxfixes-dev libxss-dev libxtst-dev
cmake -S /deployment/source -B /deployment/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DGDP_BUILD_TESTS=ON
cmake --build /deployment/build --target gdp gdp_ai_gameplay_eval gdp_worker_training_tests --parallel 1
cp /deployment/build/gdp /deployment/compiled/gdp
cp /deployment/build/gdp_ai_gameplay_eval /deployment/compiled/gdp_ai_gameplay_eval
cp /deployment/build/gdp_worker_training_tests /deployment/compiled/gdp_worker_training_tests
cp -L /deployment/build/_deps/sdl3-build/libSDL3.so.0 /deployment/compiled/libSDL3.so.0
printf 'GDP_BUILD_COMPLETE\n'

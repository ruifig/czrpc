rmdir /q /s build
md build
cd build
cmake -G "Visual Studio 15 Win64" -T "v140" ..


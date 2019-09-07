rmdir /q /s build
md build64
cd build64
cmake -G "Visual Studio 15 Win64" -T "v141" ..
cd ..


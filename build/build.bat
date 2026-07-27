g++ -O2 -march=x86-64-v2 -I ../libs/windows -c ../GRenderer2D.cpp
g++ -O2 -march=x86-64-v2 -c ../libs/windows/glad/glad.c
for /r %%i in (*.o*) do ar rcs libgrenderer2d.a %%i
del *.o*
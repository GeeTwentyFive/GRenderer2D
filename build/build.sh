g++ -O2 -march=x86-64-v2 -I ../libs/linux -c ../GRenderer2D.cpp &&
g++ -O2 -march=x86-64-v2 -c ../libs/linux/glad/glad.c &&
ar rcs libgrenderer2d.a *.o
rm *.o
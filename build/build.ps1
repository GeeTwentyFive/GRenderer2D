g++ -O2 -march=x86-64-v2 -I ../libs/windows -c ../GRenderer2D.cpp -o GRenderer2D.o
g++ -O2 -march=x86-64-v2 -c ../libs/windows/glad/glad.c -o GRenderer2D_glad.o
@"
CREATE libGRenderer2D_windows.a
ADDMOD GRenderer2D.o
ADDMOD GRenderer2D_glad.o
SAVE
END
"@ | ar -M
rm GRenderer2D.o
rm GRenderer2D_glad.o
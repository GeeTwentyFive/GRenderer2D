g++ -O2 -march=x86-64-v2 -I ../libs/linux -c ../GRenderer2D.cpp -o GRenderer2D.o &&
g++ -O2 -march=x86-64-v2 -c ../libs/linux/glad/glad.c -o GRenderer2D_glad.o &&
ar -M <<EOF
CREATE libGRenderer2D_linux.a
ADDMOD GRenderer2D.o
ADDMOD GRenderer2D_glad.o
SAVE
END
EOF
rm GRenderer2D.o GRenderer2D_glad.o
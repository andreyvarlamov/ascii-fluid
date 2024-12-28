bin/main: main.c
	clang -std=c99 -Werror -Wextra -Wall -g3 main.c third_party/glad/src/glad.c -o bin/main -lglfw -lm -Ithird_party/glad/include -Ithird_party

run: bin/main
	./bin/main

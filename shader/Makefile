obj = util.o

CC = gcc
CFLAGS = -pedantic -Wall -O1
LDFLAGS = -lGL -lGLU -lglut

.PHONY: all

raadhus_shader: raadhus_shader.o $(obj)
	$(CC) -o $@ raadhus_shader.o $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) raadhus_shader.o raadhus_shader

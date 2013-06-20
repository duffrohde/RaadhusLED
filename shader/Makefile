obj = util.o osaa.o

CC = gcc
CFLAGS = -pedantic -Wall -DGL_GLEXT_PROTOTYPES
LDFLAGS = -lGL -lGLU -lglut

.PHONY: all

raadhus_shader: raadhus_shader.o $(obj)
	$(CC) -o $@ raadhus_shader.o $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) raadhus_shader.o raadhus_shader

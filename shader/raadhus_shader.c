#include <GL/glut.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "util.h"

void init_osaa(void);
void draw_osaa(void);
void idle_func(void);
void draw(void);
void key_handler(unsigned char key, int x, int y);
void mouse_handler(int x, int y);
int read_shaders(const char *filename);

#define SCREEN_WIDTH 56
#define MAX_SHADERS 128

/*
  We set the frame time to 49 ms, which roughly corresponds to 20.4 FPS.
  The daemon should run 20 FPS thus we produce frames a bit faster than
  the playback rate. This is ok since we would rather drop frames than
  be short of frames
*/
#define FRAME_TIME 49
#define DESTINATION_HOST "192.168.2.1"
#define DESTINATION_PORT 1234

float iGlobalTime;
static int sockd;

enum e_shader_type { REAL_SHADER, GL_CODE };

static struct {
	float time;
	unsigned int prog;
	enum e_shader_type type;
} shaders[MAX_SHADERS];

static int shader_count;
static int current_shader;
static long shader_activated_time;
static long next_frame_time;
static int transition_offset_x;
static int transition_direction;

static int init_socket(void)
{
	struct sockaddr_in my_addr;

        sockd = socket(AF_INET, SOCK_DGRAM, 0);

        if (sockd == -1)
        {
                return -1;
        }

        /* Bind the socket to anything */
        my_addr.sin_family = AF_INET;
        my_addr.sin_addr.s_addr = INADDR_ANY;
        my_addr.sin_port = htons (0);

        if (bind(sockd, (struct sockaddr*)&my_addr, sizeof(my_addr)) < 0) {
		return -2;
	}

	return 0;
}

static int send_packet(void *data, int size)
{
	struct sockaddr_in dest;

	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = inet_addr(DESTINATION_HOST);
	dest.sin_port = htons (DESTINATION_PORT);

	return sendto(sockd, data, size, 0, (struct sockaddr*)&dest, sizeof(dest));
}

int main(int argc, char **argv) {
	if(init_socket() < 0) {
		perror("failed to init socket");
		return -1;
	}
	
	glutInitWindowSize(64, 60);
	
	/* initialize glut */
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutCreateWindow("Raadhus Shader");

	glutDisplayFunc(draw);
	glutIdleFunc(idle_func);
	glutKeyboardFunc(key_handler);
	glutMotionFunc(mouse_handler);
	
	if(read_shaders("shaders.conf")) {
		return EXIT_FAILURE;
	}

	set_shader(shaders[current_shader].prog);
	shader_activated_time = get_msec();

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_DEPTH_TEST);

	init_osaa();

	glutMainLoop();
	return 0;
}

void idle_func(void) {
	/* Check if we are going faster than the FRAME_TIME */
	long current_time = get_msec();
	long delta = next_frame_time - current_time;
	if(delta > 0 && delta < FRAME_TIME) {
		usleep(delta * 1000l);
		next_frame_time += FRAME_TIME;
	} else {
		/* Seems we are too late or we just started. Just sync. */
		next_frame_time = current_time + FRAME_TIME;
	}
	glutPostRedisplay();

	/* Check if we should go to the next shader or if we are transitioning */
	if(transition_offset_x) {
		transition_offset_x += transition_direction;
		if(transition_offset_x >= SCREEN_WIDTH) {
			/* Old shader is now shifted out. Switch shader and shift it in */
			current_shader = (current_shader+1) % shader_count;
			set_shader(shaders[current_shader].prog);
			shader_activated_time = current_time;

			transition_offset_x = SCREEN_WIDTH;
			transition_direction = -1;
		} else if(transition_offset_x <= 0) {
			transition_direction = 0;
		}
	} else if(shaders[current_shader].time + shader_activated_time < current_time) {
		transition_offset_x = 1;
		transition_direction = 1;
	}
}

void draw(void) {
	unsigned char pixels[SCREEN_WIDTH * 60 * 3];
	unsigned int prog = shaders[current_shader].prog;

	iGlobalTime = get_msec() / 1000.0f;

	switch(shaders[current_shader].type) {
	case GL_CODE:
		switch(prog) {
		case 1:
			draw_osaa();
			break;
		}
		break;
	case REAL_SHADER:
		set_shader(prog);

		set_uniform1f(prog, "iGlobalTime", iGlobalTime);

		glDisable(GL_BLEND);
		
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex2f(-1, -1);
		glTexCoord2f(1, 0);
		glVertex2f(1, -1);
		glTexCoord2f(1, 1);
		glVertex2f(1, 1);
		glTexCoord2f(0, 1);
		glVertex2f(-1, 1);
		glEnd();
		
		break;
	}

	set_shader(0);
	glEnable(GL_BLEND);

	glColor4f(0.0f, 0.0f, 0.0f, transition_offset_x / (float)SCREEN_WIDTH);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(-1, -1);
	glTexCoord2f(1, 0);
	glVertex2f(1, -1);
	glTexCoord2f(1, 1);
	glVertex2f(1, 1);
	glTexCoord2f(0, 1);
	glVertex2f(-1, 1);
	glEnd();

	glutSwapBuffers();

	memset(pixels, 0, SCREEN_WIDTH*60*3);
	glReadPixels(0, 0, SCREEN_WIDTH, 60, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	send_packet(pixels, sizeof(pixels));
}

void key_handler(unsigned char key, int x, int y) {
	switch(key) {
	case 27:
	case 'q':
	case 'Q':
		exit(0);
		break;
	}
}

void mouse_handler(int x, int y) {
#if 0
	int xres = glutGet(GLUT_WINDOW_WIDTH);
	int yres = glutGet(GLUT_WINDOW_HEIGHT);
#endif
}

int read_shaders(const char *filename) {
	/* If this gets more advanced we will switch to libconfuse,
	   but right now we save that dependency */
	int ret = 0;
	FILE *fd = fopen(filename, "r");
	char line[128];
	shader_count = 0;
	current_shader = 0;
	transition_offset_x = 0;

	if(!fd) {
		return -1;
	}

	while(shader_count < MAX_SHADERS && fgets(line, sizeof(line), fd)) {
		float time;
		char filename[sizeof(line)];
		if(sscanf(line, "%f %s", &time, filename) == 2) {
			unsigned int prog;
			enum e_shader_type type = REAL_SHADER;
			if(filename[0] == '/') {
				/* Special hack to address some internal GL routines */
				prog = atoi(filename+1);
				type = GL_CODE;
			} else if(!(prog = setup_shader(filename))) {
				ret = -1;
				break;
			}
			shaders[shader_count].time = time;
			shaders[shader_count].prog = prog;
			shaders[shader_count].type = type;
			shader_count++;
		}
	}

	fclose(fd);

	if(!shader_count) {
		ret = -1;
	}

	return ret;
}

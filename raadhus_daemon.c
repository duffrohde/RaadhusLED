#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#define FPS 20
#define LISTEN_PORT 1234
#define MC_GROUP "224.1.1.1"

static pthread_mutex_t screen_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t screen_cond = PTHREAD_COND_INITIALIZER;

/* We always start with 1 output buffer (which is clearing the screen) */
static int ring_buffer_head = 1;
static int ring_buffer_tail = -1;

static const int MAX_PAYLOAD_SIZE = 1472;
#define RING_BUFFER_SIZE 32
#define NUMBER_OF_PIXELS_ON_STRIP 57
#define NUMBER_OF_STRIPS_ON_PORT 4
#define NUMBER_OF_LEDS_ON_PORT (NUMBER_OF_PIXELS_ON_STRIP * NUMBER_OF_STRIPS_ON_PORT * 3)
#define PORTS_IN_USE 8

/* 32 is a nice number. Even though we have a mix of 28 and 24 we just go for 32 */
#define SEGMENT_SIZE 32
#define NUMBER_OF_SEGMENTS 2
#define XRES 56
#define SEGMENT_SIZE_BYTES (NUMBER_OF_PIXELS_ON_STRIP * 3 * SEGMENT_SIZE)

static unsigned char *screens[RING_BUFFER_SIZE][NUMBER_OF_SEGMENTS];

static void map_pixel(int ix, int iy, const unsigned char *in,
		      unsigned char *out)
{
	const unsigned char *p = &in[(ix + iy * XRES) * 3];
	out[0] = p[0];
	out[1] = p[1];
	out[2] = p[2];
}

static const unsigned char seg_maps[NUMBER_OF_SEGMENTS][SEGMENT_SIZE] =
{{12, 13, 14, 15, 16, 17, 18, 19,
  20, 21, 22, 23, 24, 25, 26, 27,
  28, 29, 30, 31, 32, 33, 34, 35,
  36, 37, 38, 39, 40, 41, 42, 42},
 {0, 1, 2, 3, 4, 5, 6, 7,
  8, 9, 10, 11, 40, 41, 42, 43,
  44, 45, 46, 47, 48, 49, 50, 51,
  52, 53, 54, 55, 56, 57, 58, 59}
};

static void map_pixels(const unsigned char *in, unsigned char *segments[])
{
	int i, ix, segment;
	for(segment = 0; segment < NUMBER_OF_SEGMENTS; segment++) {
		unsigned char *out = segments[segment];
		for (ix = 0; ix < SEGMENT_SIZE;) {
			for (i = 0; i < NUMBER_OF_STRIPS_ON_PORT; i++) {
				int ix_mapped = seg_maps[segment][ix];
				int iy;
				/* Run from bottom to top of strip */
				for (iy = 0; iy < NUMBER_OF_PIXELS_ON_STRIP; iy += 2) {
					map_pixel(ix_mapped, iy, in, out);
					out += 3;
				}
				/* Run from top to bottom of strip */
				for (iy = NUMBER_OF_PIXELS_ON_STRIP - 2; iy >= 0;
				     iy -= 2) {
					map_pixel(ix_mapped, iy, in, out);
					out += 3;
				}
				ix++;
			}
		}
	}
}

/*
  Maps a buffer to a payload and returns the number of bytes put into the payload
 */
static int payload_buffer(const unsigned char *screen, unsigned char *payload, int controller)
{
	int channelOffset = 0, ledMTUCarry = 0, byteCount = 0;
	int count = 0;

	do {
		int payloadIndex = 0;
		unsigned char *portCounter;

		if (count) {
			/* Insert dummy UDP header, which is needed since they apparently
			   did not bother to implement a full UDP stack and just throw
			   the header away
			*/
			memset(payload + payloadIndex, 0, 8);
			payload += 8;
		}

		payload[payloadIndex + 0] = 'Y';
		payload[payloadIndex + 1] = 'T';
		payload[payloadIndex + 2] = 'K';
		payload[payloadIndex + 3] = 'J';

		payload[payloadIndex + 4] = controller;
		payload[payloadIndex + 5] = 0;

		/* Unknown */
		payload[payloadIndex + 6] = 0x57;
		payload[payloadIndex + 7] = 0x05;

		portCounter = &payload[payloadIndex + 8];
		*portCounter = 0;
		payload[payloadIndex + 9] = 0;

		payloadIndex += 10;

		/* Now map the pixels */
		do {
			int bytesLeft, ledsOnPort;
			payload[payloadIndex++] = (channelOffset & 0xff);
			payload[payloadIndex++] = ((channelOffset >> 8) & 0xff);

			bytesLeft = MAX_PAYLOAD_SIZE - payloadIndex;

			if (ledMTUCarry) {
				ledsOnPort = ledMTUCarry;
			} else {
				ledsOnPort = NUMBER_OF_LEDS_ON_PORT;
			}
			if (ledsOnPort > bytesLeft) {
				/* Data cannot fit into one "MTU", we need to split it */
				ledMTUCarry = ledsOnPort - (bytesLeft - 2);
				ledsOnPort = bytesLeft - 2;
				channelOffset += ledsOnPort;
			} else {
				/* The modulus is there to compensate for data that could not fit
				   into one "MTU"
				*/
				channelOffset &= ~(2047);
				channelOffset += 2048;
				ledMTUCarry = 0;
			}

			(*portCounter)++;
			payload[payloadIndex++] = (ledsOnPort & 0xff);
			payload[payloadIndex++] = ((ledsOnPort >> 8) & 0xff);

			for (; ledsOnPort > 0; ledsOnPort--) {
				payload[payloadIndex] = *screen;
				screen++;
				payloadIndex++;
				count++;
			}
		} while (payloadIndex < MAX_PAYLOAD_SIZE
			 && count < NUMBER_OF_LEDS_ON_PORT * PORTS_IN_USE);
		payload += payloadIndex;
		byteCount += payloadIndex;
	} while (count < NUMBER_OF_LEDS_ON_PORT * PORTS_IN_USE);

	return byteCount;
}

static void *led_thread(void *data)
{
	int sockd;
	unsigned char *payload;

	struct sockaddr_in my_addr;

	/* Allocate plenty */
	payload = malloc(15000);

	sockd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockd == -1) {
		perror("Socket creation error");
		return 0;
	}

	/* Bind the socket to anything */
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	my_addr.sin_port = htons(0);

	if (bind(sockd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
		perror("failed to bind socket");
	}

	while (1) {
		int next_buffer, segment;
		pthread_mutex_lock(&screen_mutex);
		while ((next_buffer =
			(ring_buffer_tail + 1) % RING_BUFFER_SIZE) ==
		       ring_buffer_head) {
			pthread_cond_wait(&screen_cond, &screen_mutex);
		}
		ring_buffer_tail = next_buffer;

		for(segment = 0; segment < NUMBER_OF_SEGMENTS; segment++) {
			int bytes_mapped =
				payload_buffer(screens[ring_buffer_tail][segment], payload, segment + 1);
			
			struct sockaddr_in dest;
			
			dest.sin_family = AF_INET;
			dest.sin_addr.s_addr = inet_addr(MC_GROUP);
			dest.sin_port = htons(1097);
			
			if (sendto
			    (sockd, payload, bytes_mapped, 0, (struct sockaddr *)&dest,
			     sizeof(dest)) < 0) {
				/* we don't really care */
				perror("failed");
			}
		}
		pthread_mutex_unlock(&screen_mutex);
		usleep(1000 * 1000 / FPS);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	/* Buffer used to hold data from clients and the "screen" which is a mapping of the pixels
	   that fits how the LEDs should receive them */
	unsigned char *buffer;
	pthread_t tid;
	/* We need to hold xres * yres * 3, but just allocate plenty */
	const int buffer_size = 15000;
	struct sockaddr_in my_addr, client_addr;
	socklen_t addrlen;
	ssize_t bread;
	int sockd;

	/* malloc the receiving buffer */
	buffer = malloc(buffer_size);
	/* malloc all the temporary screens */
	{
		int i, j;
		for(i = 0; i < RING_BUFFER_SIZE; i++) {
			for(j = 0; j < NUMBER_OF_SEGMENTS; j++) {
				screens[i][j] = calloc(1, SEGMENT_SIZE_BYTES);
			}
		}
	}

	sockd = socket(AF_INET, SOCK_DGRAM, 0);

	if (sockd == -1) {
		perror("Socket creation error");
		return -2;
	}

	/* Start LED thread */
	pthread_create(&tid, NULL, led_thread, NULL);
	pthread_detach(tid);

	/* Bind the socket to our listening port */
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	my_addr.sin_port = htons(LISTEN_PORT);

	bind(sockd, (struct sockaddr *)&my_addr, sizeof(my_addr));

	addrlen = sizeof(client_addr);

	while ((bread =
		recvfrom(sockd, buffer, buffer_size, 0,
			 (struct sockaddr *)&client_addr, &addrlen)) >= 0) {
		addrlen = sizeof(client_addr);
		pthread_mutex_lock(&screen_mutex);
		/* Only use the received buffer if output to LEDs is up to speed */
		if (ring_buffer_head != ring_buffer_tail) {
			map_pixels(buffer, screens[ring_buffer_head]);
			ring_buffer_head =
			    (ring_buffer_head + 1) % RING_BUFFER_SIZE;
			pthread_cond_broadcast(&screen_cond);
		}
		pthread_mutex_unlock(&screen_mutex);
	}

	return 0;
}

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

#define FPS 10
#define LISTEN_PORT 1234
#define MC_GROUP "224.1.1.1"

/* Used to hold the "screen" sent from clients */
static unsigned char *buffer;

static const int MAX_PAYLOAD_SIZE = 1472;
static const int CONTROLLER = 2;
#define NUMBER_OF_PIXELS_ON_STRIP 57
#define NUMBER_OF_STRIPS_ON_PORT 4
#define NUMBER_OF_LEDS_ON_PORT (NUMBER_OF_PIXELS_ON_STRIP * NUMBER_OF_STRIPS_ON_PORT * 3)
#define PORTS_IN_USE 8

/* 32 is a nice number. Even though we only have an xRes of 30 we use 32 */
static int xRes = 32;

static void map_pixel(int ix, int iy, const unsigned char *screen,
		      unsigned char *mapped) {
	const unsigned char *p = &screen[(ix + iy * xRes) * 3];
	mapped[0] = p[0];
	mapped[1] = p[1];
	mapped[2] = p[2];
}

static void map_pixels(const unsigned char *buffer, unsigned char *temp)
{
	int i, ix;
	for(ix = 0; ix < xRes;) {
		for (i = 0; i < NUMBER_OF_STRIPS_ON_PORT; i++) {
			int iy;
			/* Run from bottom to top of strip */
			for (iy = 0; iy < NUMBER_OF_PIXELS_ON_STRIP; iy += 2) {
				map_pixel(ix, iy, buffer, temp);
				temp += 3;
			}
			/* Run from top to bottom of strip */
			for (iy = NUMBER_OF_PIXELS_ON_STRIP - 2; iy >= 0; iy -= 2) {
				map_pixel(ix, iy, buffer, temp);
				temp += 3;
			}
			ix++;
		}
	}
}

/*
  Maps a buffer to a payload and returns the number of bytes put into the payload
 */
static int payload_buffer(const unsigned char *buffer, unsigned char *temp, unsigned char *payload)
{
	int channelOffset = 0, ledMTUCarry = 0, byteCount = 0;
	int count = 0;

	map_pixels(buffer, temp);
	
	do {
		int payloadIndex = 0;
		unsigned char *portCounter;

		if(count) {
			// Insert dummy UDP header, which is needed since they apparently
			// did not bother to implement a full UDP stack and just throw
			// the header away
			memset(payload+payloadIndex, 0, 8);
			payload += 8;
		}

		payload[payloadIndex+0] = 'Y';
		payload[payloadIndex+1] = 'T';
		payload[payloadIndex+2] = 'K';
		payload[payloadIndex+3] = 'J';
		
		payload[payloadIndex+4] = CONTROLLER;
		payload[payloadIndex+5] = 0;
		
		// Unknown
		payload[payloadIndex+6] = 0x57;
		payload[payloadIndex+7] = 0x05;
		
		portCounter = &payload[payloadIndex+8];
		*portCounter = 0;
		payload[payloadIndex+9] = 0;
		
		payloadIndex += 10;
	
		// Now map the pixels
		do {
			payload[payloadIndex++] = (channelOffset & 0xff);
			payload[payloadIndex++] = ((channelOffset >> 8) & 0xff);

			int bytesLeft = MAX_PAYLOAD_SIZE - payloadIndex;
			int ledsOnPort;

			if(ledMTUCarry) {
				ledsOnPort = ledMTUCarry;
			} else {
				ledsOnPort = NUMBER_OF_LEDS_ON_PORT;
			}
			if(ledsOnPort > bytesLeft) {
				// Data cannot fit into one "MTU", we need to split it
				ledMTUCarry = ledsOnPort - (bytesLeft - 2);
				ledsOnPort = bytesLeft - 2;
				channelOffset += ledsOnPort;
			} else {
				// The modulus is there to compensate for data that could not fit
				// into one "MTU"
				channelOffset &= ~(2047);
				channelOffset += 2048;
				ledMTUCarry = 0;
			}

			(*portCounter)++;
			payload[payloadIndex++] = (ledsOnPort & 0xff);
			payload[payloadIndex++] = ((ledsOnPort >> 8) & 0xff);

			for(; ledsOnPort > 0; ledsOnPort--) {
				payload[payloadIndex] = *temp;
				temp++;
				payloadIndex++;
				count++;
			}
		} while(payloadIndex < MAX_PAYLOAD_SIZE && count < NUMBER_OF_LEDS_ON_PORT * PORTS_IN_USE);
		payload += payloadIndex;
		byteCount += payloadIndex;
	} while(count < NUMBER_OF_LEDS_ON_PORT * PORTS_IN_USE);

	return byteCount;
}

static void *led_thread (void *data)
{
	int sockd;
	unsigned char *payload, *temp;
	struct sockaddr_in my_addr;

	/* Allocate plenty */
	payload = malloc(15000);
	temp = malloc(15000);

        sockd = socket(AF_INET, SOCK_DGRAM, 0);

        if (sockd == -1)
        {
                perror("Socket creation error");
                return 0;
        }

        /* Bind the socket to anything */
        my_addr.sin_family = AF_INET;
        my_addr.sin_addr.s_addr = INADDR_ANY;
        my_addr.sin_port = htons (0);
        
        if (bind(sockd, (struct sockaddr*)&my_addr, sizeof(my_addr)) < 0) {
		perror("failed to bind socket");
	}

	while(1) {
		int bytes_mapped = payload_buffer(buffer, temp, payload);
		struct sockaddr_in dest;
		
		dest.sin_family = AF_INET;
		dest.sin_addr.s_addr = inet_addr(MC_GROUP);
		dest.sin_port = htons (1097);
		
		if(sendto(sockd, payload, bytes_mapped, 0, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
			/* we don't really care */
			perror("failed");
		}
		usleep(1000*1000 / FPS);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	pthread_t tid;
	/* We need to hold xres * yres * 3, but just allocate plenty */
	const int buffer_size = 15000;
        struct sockaddr_in my_addr, client_addr;
        socklen_t addrlen;
	ssize_t bread;
	int sockd, x, y;

	buffer = malloc(buffer_size);

        sockd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockd == -1)
        {
                perror("Socket creation error");
                return -2;
        }

	/* Start LED thread */
	pthread_create (&tid, NULL, led_thread, NULL);
	pthread_detach (tid);
        
        /* Bind the socket to our listening port */
        my_addr.sin_family = AF_INET;
        my_addr.sin_addr.s_addr = INADDR_ANY;
        my_addr.sin_port = htons (LISTEN_PORT);
        
        bind(sockd, (struct sockaddr*)&my_addr, sizeof(my_addr));
        
        addrlen = sizeof(client_addr);
	
	for(x=0; x<xRes; x++) {
		for(y=0; y<NUMBER_OF_PIXELS_ON_STRIP; y++) {
			unsigned char *p = &buffer[(y*xRes + x) * 3];
			p[0] = y;
			p[1] = x;
			p[2] = 0;
		}
	}

	/* Receive directly into "screen" buffer */
        while ((bread = recvfrom (sockd, buffer, buffer_size, 0, (struct sockaddr*)&client_addr, &addrlen)) >= 0) {
                addrlen = sizeof(client_addr);
        }

	return 0;
}

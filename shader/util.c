#if defined(__unix__) || defined(unix)
#include <time.h>
#include <sys/time.h>
#else	/* assume win32 */
#include <windows.h>
#endif	/* __unix__ */

#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>

static int load_shader(const char *fname, GLenum type) {
	FILE *fp;
	GLhandleARB sdr;
	unsigned int len;
	char *src_buf;
	int success;

	if(!(fp = fopen(fname, "r"))) {
		fprintf(stderr, "failed to open shader: %s\n", fname);
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	src_buf = malloc(len + 1);

	fread(src_buf, 1, len, fp);
	src_buf[len] = 0;

	sdr = glCreateShaderObjectARB(type);
	glShaderSourceARB(sdr, 1, (const char**)&src_buf, 0);
	free(src_buf);

	glCompileShaderARB(sdr);
	glGetObjectParameterivARB(sdr, GL_OBJECT_COMPILE_STATUS_ARB, &success);
	if(!success) {
		int info_len;
		char *info_log;
		
		glGetObjectParameterivARB(sdr, GL_OBJECT_INFO_LOG_LENGTH_ARB, &info_len);
		if(info_len > 0) {
			if(!(info_log = malloc(info_len + 1))) {
				perror("malloc failed");
				return 0;
			}
			glGetInfoLogARB(sdr, info_len, 0, info_log);
			fprintf(stderr, "shader compilation failed: %s\n", info_log);
			free(info_log);
		} else {
			fprintf(stderr, "shader compilation failed\n");
		}
		return 0;
	}

	return sdr;
}

unsigned int setup_shader(const char *fname) {
	unsigned int prog, sdr;
	int linked;

	sdr = load_shader(fname, GL_FRAGMENT_SHADER_ARB);
	if(!sdr) {
		fprintf(stderr, "shader loading failed\n");
		return 0;
	}

	prog = glCreateProgramObjectARB();
	glAttachObjectARB(prog, sdr);
	glLinkProgramARB(prog);
	glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &linked);
	if(!linked) {
		fprintf(stderr, "shader linking failed\n");
		return 0;
	}

	return prog;
}

unsigned int setup_shader_vertex(const char *fname_frag, const char *fname_vertex) {
	unsigned int prog, sdr_frag, sdr_vertex;
	int linked;

	sdr_frag = load_shader(fname_frag, GL_FRAGMENT_SHADER_ARB);
	if(!sdr_frag) {
		fprintf(stderr, "shader loading failed\n");
		return 0;
	}

	sdr_vertex = load_shader(fname_vertex, GL_VERTEX_SHADER_ARB);
	if(!sdr_vertex) {
		fprintf(stderr, "shader loading failed\n");
		return 0;
	}

	prog = glCreateProgramObjectARB();
	glAttachObjectARB(prog, sdr_frag);
	glAttachObjectARB(prog, sdr_vertex);
	glLinkProgramARB(prog);
	glGetObjectParameterivARB(prog, GL_OBJECT_LINK_STATUS_ARB, &linked);
	if(!linked) {
		fprintf(stderr, "shader linking failed\n");
		return 0;
	}

	return prog;
}

void set_uniform1f(unsigned int prog, const char *name, float val) {
	int loc = glGetUniformLocationARB(prog, name);
	if(loc != -1) {
		glUniform1f(loc, val);
	}
}

void set_uniform2f(unsigned int prog, const char *name, float v1, float v2) {
	int loc = glGetUniformLocationARB(prog, name);
	if(loc != -1) {
		glUniform2f(loc, v1, v2);
	}
}

void set_uniform1i(unsigned int prog, const char *name, int val) {
	int loc = glGetUniformLocationARB(prog, name);
	if(loc != -1) {
		glUniform1i(loc, val);
	}
}

void set_shader(unsigned int prog) {
	glUseProgramObjectARB(prog);
}

unsigned long get_msec(void) {
#if defined(__unix__) || defined(unix)
	static struct timeval timeval, first_timeval;
	
	gettimeofday(&timeval, 0);

	if(first_timeval.tv_sec == 0) {
		first_timeval = timeval;
		return 0;
	}
	return (timeval.tv_sec - first_timeval.tv_sec) * 1000 + (timeval.tv_usec - first_timeval.tv_usec) / 1000;
#else
	return GetTickCount();
#endif	/* __unix__ */
}

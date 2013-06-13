#if defined(__unix__) || defined(unix)
#include <time.h>
#include <sys/time.h>
#else	/* assume win32 */
#include <windows.h>
#endif	/* __unix__ */

#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>

#ifdef __unix__
GLhandleARB glCreateShaderObjectARB(GLenum);
void glShaderSourceARB(GLhandleARB, int, const char**, int*);
void glCompileShaderARB(GLhandleARB);
GLhandleARB glCreateProgramObjectARB(void);
void glAttachObjectARB(GLhandleARB, GLhandleARB);
void glLinkProgramARB(GLhandleARB);
void glUseProgramObjectARB(GLhandleARB);
void glGetInfoLogARB(GLhandleARB, GLsizei, GLsizei*, GLcharARB*);
void glGetObjectParameterivARB(GLhandleARB, GLenum, int*);
GLint glGetUniformLocationARB(GLhandleARB, const char*);
void glUniform1f(GLint location, GLfloat v0);
void glUniform1i(GLint location, GLint v0);
void glUniform2f(GLint location, GLfloat v0, GLfloat v1);
#endif

unsigned int setup_shader(const char *fname) {
	FILE *fp;
	unsigned int prog, sdr, len;
	char *src_buf;
	int success, linked;

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

	sdr = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
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

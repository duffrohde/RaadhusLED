#include <GL/glut.h>
#include <GL/glext.h>
#include <math.h>

#include "util.h"
#include "osaa_logo.h"

extern float iGlobalTime;

static GLuint *vbo = NULL;
static GLuint *vinx = NULL;
static unsigned int osaa_shader;

void init_osaa() {
	unsigned int i;
	
	vbo = (GLuint *)malloc(OBJECTS_COUNT * sizeof(GLuint));
	vinx = (GLuint *)malloc(OBJECTS_COUNT * sizeof(GLuint));
	
	glGenBuffers(OBJECTS_COUNT, vbo);
	
	for (i=0; i<OBJECTS_COUNT; i++) {     
		glBindBuffer(GL_ARRAY_BUFFER, vbo[i]);
		glBufferData(GL_ARRAY_BUFFER, sizeof (struct vertex_struct) * vertex_count[i], &vertices[vertex_offset_table[i]], GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
	
	glGenBuffers(OBJECTS_COUNT, vinx);
	for (i=0; i<OBJECTS_COUNT; i++) {     
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vinx[i]);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof (indexes[0]) * faces_count[i] * 3, &indexes[indices_offset_table[i]], GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	osaa_shader = setup_shader_vertex("osaa_frag.glsl", "osaa_vertex.glsl");
}

#define BUFFER_OFFSET(x)((char *)NULL+(x))

void draw_osaa(void)
{
	int index = 0, i;

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	set_shader(osaa_shader);
	
	glPushMatrix();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

       	glScalef(22.0f, 22.0f, 22.0f);
	glRotatef(iGlobalTime * 100.0f, 8.0f, 3.0f, 0.0f);
		
	glMatrixMode(GL_MODELVIEW);

	for (i=0; i<6; i++) {
		float angle = i * 360.0f / 6.0f + iGlobalTime * 100.0f;

		glLoadIdentity();
		glTranslatef(cosf(3.14f * angle / 180.0f) / 46.5f, sinf(3.14f * angle / 180.0f) / 46.5f, 0.0f);
		glRotatef(angle + -72.0f, 0.0f, 0.0f, 1.0f);
		glBindBuffer(GL_ARRAY_BUFFER, vbo[index]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vinx[index]);

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(3, GL_FLOAT, sizeof (struct vertex_struct), BUFFER_OFFSET(0));

		glEnableClientState(GL_NORMAL_ARRAY);
		glNormalPointer(GL_FLOAT, sizeof (struct vertex_struct), BUFFER_OFFSET(3 * sizeof (float)));

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, sizeof (struct vertex_struct), BUFFER_OFFSET(6 * sizeof (float)));

		glDrawElements(GL_TRIANGLES, faces_count[index] * 3, INX_TYPE, BUFFER_OFFSET(0));

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
	}
		  
	glPopMatrix();
}

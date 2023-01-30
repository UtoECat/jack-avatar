// Copyright (C) UtoECat. All rights reserved.
// GNU GPL 3.0 License.
// No any warrianty.

#include <jack/jack.h>
#include <galogen.h>
#include <GLFW/glfw3.h>
#include <stddef.h>
#include <stdio.h>
#include <fft.h>
#include <stb_image.h>
#include <malloc.h>
#include <math.h>

GLFWwindow* win;
jack_client_t* J;
jack_port_t* P;

#define NAME "AVATARRR"

static void error_callback(int error, const char* description) {
    fprintf(stderr, "Error: %s\n", description);
}

static float volume = 0;
static float powerup = 12;
static GLuint mouthTextures[5];
static GLuint headTexture;
static GLuint eyesTexture;

static float wgausse(float v, float s) {
	float a = (s-1)/2.0;
	float t = (v-a)/(0.5*a);
	t *= t;
	return exp(-t/2);
}

static inline float logn(float v, float n) {return log10(v)/log10(n);}
static inline float logone(float v, float n) {return logn(v*n + 1, n)/logn(n + 1, n);}

int process (jack_nframes_t cnt, void*) {
	float* srcold = (float*) jack_port_get_buffer(P, cnt);
	float  src[cnt];
	float  tmp[cnt];

	for (size_t i = 0; i < cnt; i++) {
		src[i] = srcold[i] * wgausse(i, cnt);
		tmp[i] = 0;
	}

	FFT(src, tmp, cnt, getLogN(cnt), FT_DIRECT);
	// move back
	size_t maxcnt = cnt/2;
	float max = 0.0f;
	for (size_t i = 0; i < maxcnt; i++) {
		float k = maxcnt-i;
		if (k < 0) k = 0;
		k /= maxcnt;

		float v = (src[i]/cnt*2*3.14) * k;
		if (v < 0) v = -v;
		if (v > max) max = (max*3 + v) / 4.0;
	}
	volume = (volume * 3 + max) / 4.0;
	return 0;
}

GLuint LoadTexture(const char* filename) {
	int w, h, ch;
	GLenum fmt = GL_RGB;
	GLuint obj = 0;

	unsigned char *data = stbi_load(filename, &w, &h, &ch, 0);
	switch (ch) {
		case 1: fmt = GL_RED;  break;
		case 2: fmt = GL_RG;   break;
		case 3: fmt = GL_RGB;  break;
		case 4: fmt = GL_RGBA; break;
		default: goto error;
	}
	if (data) {
		glGenTextures(1, &obj);
		if (!obj) goto error;
		glBindTexture(GL_TEXTURE_2D, obj);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
		fprintf(stderr, "Loaded image %s (w %i, h %i, ch %i)!\n", filename, w, h, ch);
	} else {
		error:
		fprintf(stderr, "Can't load image %s!\n", filename);
	}
	free(data);
	return obj;
}

void freeTexture(GLuint obj) {
	glDeleteTextures(1, &obj);
}

void drawTexture(GLuint obj, float x, float y) {
	glBindTexture(GL_TEXTURE_2D, obj);
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(x-1.0, y+1.0);
	glTexCoord2f(1, 0);
	glVertex2f(x+1.0, y+1.0);
	glTexCoord2f(1, 1);
	glVertex2f(x+1.0, y-1.0);
	glTexCoord2f(0, 1);
	glVertex2f(x-1.0, y-1.0);
	glEnd();
}


#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

void getMouseGlobalFill(float* kx, float* ky) {
	Display* XD = glfwGetX11Display();
	if (!XD) return;
	int root_x, root_y;

	int XS = DefaultScreen(XD);
	int width = DisplayWidth(XD, XS);
	int height = DisplayHeight(XD, XS);
	Window XW = XRootWindow(XD, XS);

	Window unused; int unusedi; unsigned int unusedui;

	if (XQueryPointer(XD, XW, &unused, &unused,
			&root_x, &root_y, &unusedi, &unusedi, &unusedui)) {
		*kx = root_x /(float)width;
		*ky = root_y /(float)height;
	} else {
		*kx = 0;
		*ky = 0;
	}
}

int main(void) {
	glfwSetErrorCallback(error_callback);
 	if (!glfwInit()) return -1;
	
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, GALOGEN_API_VER_MAJ);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, GALOGEN_API_VER_MIN);
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
	glfwWindowHint(GLFW_ALPHA_BITS, GL_TRUE);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

	win = glfwCreateWindow(512, 512, NAME, NULL, NULL);
	if (!win) {
		glfwTerminate();
		return -2;
	}
	glfwMakeContextCurrent(win);
	glfwSwapInterval(1);
	
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_COLOR_MATERIAL);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glEnable(GL_ALPHA_TEST);
	// load textures!
	eyesTexture = LoadTexture("eyes.png");
	headTexture = LoadTexture("head.png");
	mouthTextures[0] = LoadTexture("mouth0.png");
	mouthTextures[1] = LoadTexture("mouth1.png");
	mouthTextures[2] = LoadTexture("mouth2.png");
	mouthTextures[3] = LoadTexture("mouth3.png");
	mouthTextures[4] = LoadTexture("mouth4.png");

	J = jack_client_open(NAME, JackNullOption, NULL);
	if (!J) {
		return -3;
	}
	
	jack_set_process_callback(J, process, NULL);
	P = jack_port_register(J, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput | JackPortIsTerminal, 0);
	jack_activate(J);
	
	while (!glfwWindowShouldClose(win)) {
		int width, height;
		glfwGetFramebufferSize(win, &width, &height);
 
		glViewport(0, 0, width, height);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity();

		// get global mouse pos
		float kx, ky;
		getMouseGlobalFill(&kx, &ky);
		ky = (ky-0.5) * -0.15;
		kx = (kx-0.5) * 0.15;

		// get volume
		float vol = volume;
		float iff = logone(vol, 50) * powerup;
		float distor = 0.0f;
		if (iff < 0) iff = 0;
		if (iff > 4) {
			distor = (iff - 6)/(powerup-6);
			iff = 4;
		}
		int i = iff;


		if (distor > 0.001) {
			float distor2 = (1-distor);
			glColor4f(1,distor2,distor2,1);
		} else
			glColor4f(1,1,1,1);

		if (distor > 0.001) {
			float rndx = (rand()%2048)/2047.0f - 0.5;
			float rndy = (rand()%2048)/2047.0f - 0.5;
			rndx *= distor * 0.2;
			rndy *= distor * 0.2;
			glTranslatef(rndx, rndy, 0);
		}

		drawTexture(headTexture, 0, 0);
		drawTexture(eyesTexture, kx, ky);
		drawTexture(mouthTextures[i], 0, 0);

		glfwSwapBuffers(win);
		glfwPollEvents();
	}
 
	jack_deactivate(J);
	jack_port_unregister(J, P);
	jack_client_close(J);

	glfwDestroyWindow(win);
 	glfwTerminate();
	return 0;
}

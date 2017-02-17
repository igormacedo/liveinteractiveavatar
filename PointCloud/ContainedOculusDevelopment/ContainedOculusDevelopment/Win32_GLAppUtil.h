/************************************************************************************
Filename    :   Win32_GLAppUtil.h
Content     :   OpenGL and Application/Window setup functionality for RoomTiny
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, LLC. All Rights reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/

#include "btBulletDynamicsCommon.h"
#include "LibOVRKernel/Src/GL/CAPI_GLE.h"
#include "LibOVR/Include/Extras/OVR_Math.h"
#include "LibOVRKernel/Src/Kernel/OVR_Log.h"
#include "LibOVR/Include/OVR_CAPI_GL.h"
#include "LibOVRKernel/Src/Kernel/OVR_Types.h"
#include "KinectHandler.h"
#include <iostream>

#define screen_width 1024
#define screen_height 848
#define color_width 1920
#define color_height 1080
#define depth_width 512
#define depth_height 424

using namespace OVR;
using namespace std;

KinectHandler* kinect;
btDiscreteDynamicsWorld* dynamicsWorld;

#ifndef VALIDATE
#define VALIDATE(x, msg) if (!(x)) { MessageBoxA(NULL, (msg), "OculusRoomTiny", MB_ICONERROR | MB_OK); exit(-1); }
#endif

//--------------------------------------------------------------------------
struct DepthBuffer
{
	GLuint        texId;

	DepthBuffer(Sizei size, int sampleCount)
	{
		OVR_ASSERT(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

		glGenTextures(1, &texId);
		glBindTexture(GL_TEXTURE_2D, texId);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		GLenum internalFormat = GL_DEPTH_COMPONENT24;
		GLenum type = GL_UNSIGNED_INT;
		if (GLE_ARB_depth_buffer_float)
		{
			internalFormat = GL_DEPTH_COMPONENT32F;
			type = GL_FLOAT;
		}

		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, size.w, size.h, 0, GL_DEPTH_COMPONENT, type, NULL);
	}
	~DepthBuffer()
	{
		if (texId)
		{
			glDeleteTextures(1, &texId);
			texId = 0;
		}
	}
};

//--------------------------------------------------------------------------
struct TextureBuffer
{
	ovrHmd              hmd;
	ovrSwapTextureSet*  TextureSet;
	GLuint              texId;
	GLuint              fboId;
	Sizei               texSize;

	TextureBuffer(ovrHmd hmd, bool rendertarget, bool displayableOnHmd, Sizei size, int mipLevels, unsigned char * data, int sampleCount) :
		hmd(hmd),
		TextureSet(nullptr),
		texId(0),
		fboId(0),
		texSize(0, 0)
	{
		OVR_ASSERT(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

		texSize = size;

		if (displayableOnHmd)
		{
			// This texture isn't necessarily going to be a rendertarget, but it usually is.
			OVR_ASSERT(hmd); // No HMD? A little odd.
			OVR_ASSERT(sampleCount == 1); // ovr_CreateSwapTextureSetD3D11 doesn't support MSAA.

			ovrResult result = ovr_CreateSwapTextureSetGL(hmd, GL_SRGB8_ALPHA8, size.w, size.h, &TextureSet);

			if (OVR_SUCCESS(result))
			{
				for (int i = 0; i < TextureSet->TextureCount; ++i)
				{
					ovrGLTexture* tex = (ovrGLTexture*)&TextureSet->Textures[i];
					glBindTexture(GL_TEXTURE_2D, tex->OGL.TexId);

					if (rendertarget)
					{
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
					}
					else
					{
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
					}
				}
			}
		}
		else
		{
			glGenTextures(1, &texId);
			glBindTexture(GL_TEXTURE_2D, texId);

			if (rendertarget)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			}

			glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, texSize.w, texSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		}

		if (mipLevels > 1)
		{
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		glGenFramebuffers(1, &fboId);
	}

	~TextureBuffer()
	{
		if (TextureSet)
		{
			ovr_DestroySwapTextureSet(hmd, TextureSet);
			TextureSet = nullptr;
		}
		if (texId)
		{
			glDeleteTextures(1, &texId);
			texId = 0;
		}
		if (fboId)
		{
			glDeleteFramebuffers(1, &fboId);
			fboId = 0;
		}
	}

	Sizei GetSize() const
	{
		return texSize;
	}

	void SetAndClearRenderSurface(DepthBuffer* dbuffer)
	{
		auto tex = reinterpret_cast<ovrGLTexture*>(&TextureSet->Textures[TextureSet->CurrentIndex]);

		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->OGL.TexId, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dbuffer->texId, 0);

		glViewport(0, 0, texSize.w, texSize.h);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_FRAMEBUFFER_SRGB);
	}

	void UnsetRenderSurface()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fboId);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
	}
};

//---------------------------------------------------------------------------
struct OGL
{
	static const bool       UseDebugContext = false;

	HWND                    Window;
	HDC                     hDC;
	HGLRC                   WglContext;
	OVR::GLEContext         GLEContext;
	bool                    Running;
	bool                    Key[256];
	int                     WinSizeW;
	int                     WinSizeH;
	GLuint                  fboId;
	HINSTANCE               hInstance;

	static LRESULT CALLBACK WindowProc(_In_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
	{
		OGL *p = reinterpret_cast<OGL *>(GetWindowLongPtr(hWnd, 0));
		switch (Msg)
		{
		case WM_KEYDOWN:
			p->Key[wParam] = true;
			break;
		case WM_KEYUP:
			p->Key[wParam] = false;
			break;
		case WM_DESTROY:
			p->Running = false;
			break;
		default:
			return DefWindowProcW(hWnd, Msg, wParam, lParam);
		}
		if ((p->Key['Q'] && p->Key[VK_CONTROL]) || p->Key[VK_ESCAPE])
		{
			p->Running = false;
		}
		return 0;
	}

	OGL() :
		Window(nullptr),
		hDC(nullptr),
		WglContext(nullptr),
		GLEContext(),
		Running(false),
		WinSizeW(0),
		WinSizeH(0),
		fboId(0),
		hInstance(nullptr)
	{
		// Clear input
		for (int i = 0; i < sizeof(Key) / sizeof(Key[0]); ++i)
			Key[i] = false;
	}

	~OGL()
	{
		ReleaseDevice();
		CloseWindow();
	}

	bool InitWindow(HINSTANCE hInst, LPCWSTR title)
	{
		hInstance = hInst;
		Running = true;

		WNDCLASSW wc;
		memset(&wc, 0, sizeof(wc));
		wc.style = CS_CLASSDC;
		wc.lpfnWndProc = WindowProc;
		wc.cbWndExtra = sizeof(struct OGL *);
		wc.hInstance = GetModuleHandleW(NULL);
		wc.lpszClassName = L"ORT";
		RegisterClassW(&wc);

		// adjust the window size and show at InitDevice time
		Window = CreateWindowW(wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, 0, 0, hInstance, 0);
		if (!Window) return false;

		SetWindowLongPtr(Window, 0, LONG_PTR(this));

		hDC = GetDC(Window);

		return true;
	}

	void CloseWindow()
	{
		if (Window)
		{
			if (hDC)
			{
				ReleaseDC(Window, hDC);
				hDC = nullptr;
			}
			DestroyWindow(Window);
			Window = nullptr;
			UnregisterClassW(L"OGL", hInstance);
		}
	}

	// Note: currently there is no way to get GL to use the passed pLuid
	bool InitDevice(int vpW, int vpH, const LUID* /*pLuid*/, bool windowed = true)
	{
		WinSizeW = vpW;
		WinSizeH = vpH;

		RECT size = { 0, 0, vpW, vpH };
		AdjustWindowRect(&size, WS_OVERLAPPEDWINDOW, false);
		const UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_SHOWWINDOW;
		if (!SetWindowPos(Window, nullptr, 0, 0, size.right - size.left, size.bottom - size.top, flags))
			return false;

		PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARBFunc = nullptr;
		PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARBFunc = nullptr;
		{
			// First create a context for the purpose of getting access to wglChoosePixelFormatARB / wglCreateContextAttribsARB.
			PIXELFORMATDESCRIPTOR pfd;
			memset(&pfd, 0, sizeof(pfd));
			pfd.nSize = sizeof(pfd);
			pfd.nVersion = 1;
			pfd.iPixelType = PFD_TYPE_RGBA;
			pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
			pfd.cColorBits = 32;
			pfd.cDepthBits = 16;
			int pf = ChoosePixelFormat(hDC, &pfd);
			VALIDATE(pf, "Failed to choose pixel format.");

			VALIDATE(SetPixelFormat(hDC, pf, &pfd), "Failed to set pixel format.");

			HGLRC context = wglCreateContext(hDC);
			VALIDATE(context, "wglCreateContextfailed.");
			VALIDATE(wglMakeCurrent(hDC, context), "wglMakeCurrent failed.");

			wglChoosePixelFormatARBFunc = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
			wglCreateContextAttribsARBFunc = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
			OVR_ASSERT(wglChoosePixelFormatARBFunc && wglCreateContextAttribsARBFunc);

			wglDeleteContext(context);
		}

		// Now create the real context that we will be using.
		int iAttributes[] =
		{
			// WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
			WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
			WGL_COLOR_BITS_ARB, 32,
			WGL_DEPTH_BITS_ARB, 16,
			WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
			WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
			0, 0
		};

		float fAttributes[] = { 0, 0 };
		int   pf = 0;
		UINT  numFormats = 0;

		VALIDATE(wglChoosePixelFormatARBFunc(hDC, iAttributes, fAttributes, 1, &pf, &numFormats),
			"wglChoosePixelFormatARBFunc failed.");

		PIXELFORMATDESCRIPTOR pfd;
		memset(&pfd, 0, sizeof(pfd));
		VALIDATE(SetPixelFormat(hDC, pf, &pfd), "SetPixelFormat failed.");

		GLint attribs[16];
		int   attribCount = 0;
		if (UseDebugContext)
		{
			attribs[attribCount++] = WGL_CONTEXT_FLAGS_ARB;
			attribs[attribCount++] = WGL_CONTEXT_DEBUG_BIT_ARB;
		}

		attribs[attribCount] = 0;

		WglContext = wglCreateContextAttribsARBFunc(hDC, 0, attribs);
		VALIDATE(wglMakeCurrent(hDC, WglContext), "wglMakeCurrent failed.");

		OVR::GLEContext::SetCurrentContext(&GLEContext);
		GLEContext.Init();

		glGenFramebuffers(1, &fboId);

		glEnable(GL_DEPTH_TEST);
		glFrontFace(GL_CW);
		glEnable(GL_CULL_FACE);

		if (UseDebugContext && GLE_ARB_debug_output)
		{
			glDebugMessageCallbackARB(DebugGLCallback, NULL);
			if (glGetError())
			{
				OVR_DEBUG_LOG(("glDebugMessageCallbackARB failed."));
			}

			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);

			// Explicitly disable notification severity output.
			glDebugMessageControlARB(GL_DEBUG_SOURCE_API, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
		}

		return true;
	}

	bool HandleMessages(void)
	{
		MSG msg;
		while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		return Running;
	}

	void Run(bool(*MainLoop)(bool retryCreate))
	{
		// false => just fail on any error
		VALIDATE(MainLoop(false), "Oculus Rift not detected.");
		while (HandleMessages())
		{
			// true => we'll attempt to retry for ovrError_DisplayLost
			if (!MainLoop(true))
				break;
			// Sleep a bit before retrying to reduce CPU load while the HMD is disconnected
			Sleep(10);
		}
	}

	void ReleaseDevice()
	{
		if (fboId)
		{
			glDeleteFramebuffers(1, &fboId);
			fboId = 0;
		}
		if (WglContext)
		{
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(WglContext);
			WglContext = nullptr;
		}
		GLEContext.Shutdown();
	}

	static void GLAPIENTRY DebugGLCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
	{
		OVR_DEBUG_LOG(("Message from OpenGL: %s\n", message));
	}
};

// Global OpenGL state
static OGL Platform;

//---------------------------------------------------------------------------
struct ShaderFill
{
	GLuint            program;
	TextureBuffer   * texture;

	ShaderFill(GLuint vertexShader, GLuint pixelShader, TextureBuffer* _texture)
	{
		texture = _texture;

		program = glCreateProgram();

		glAttachShader(program, vertexShader);
		glAttachShader(program, pixelShader);

		glLinkProgram(program);

		glDetachShader(program, vertexShader);
		glDetachShader(program, pixelShader);

		GLint r;
		glGetProgramiv(program, GL_LINK_STATUS, &r);
		if (!r)
		{
			GLchar msg[1024];
			glGetProgramInfoLog(program, sizeof(msg), 0, msg);
			OVR_DEBUG_LOG(("Linking shaders failed: %s\n", msg));
		}
	}

	~ShaderFill()
	{
		if (program)
		{
			glDeleteProgram(program);
			program = 0;
		}
		if (texture)
		{
			delete texture;
			texture = nullptr;
		}
	}
};

//---------------------------------------------------------------------------
struct VertexBuffer
{
	GLuint    buffer;

	VertexBuffer(void* vertices, size_t size)
	{
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}
	~VertexBuffer()
	{
		if (buffer)
		{
			glDeleteBuffers(1, &buffer);
			buffer = 0;
		}
	}
};

//---------------------------------------------------------------------------
struct IndexBuffer
{
	GLuint    buffer;

	IndexBuffer(void* indices, size_t size)
	{
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
	}
	~IndexBuffer()
	{
		if (buffer)
		{
			glDeleteBuffers(1, &buffer);
			buffer = 0;
		}
	}
};

//---------------------------------------------------------------------------

struct Model
{
	struct Vertex
	{
		Vector3f  Pos;
		DWORD     C;
		float     U, V;
	};

	Vector3f        Pos;
	Quatf           Rot;
	Matrix4f        Mat;
	int             numVertices, numIndices;
	Vertex          Vertices[2000]; // Note fixed maximum
	GLushort        Indices[2000];
	ShaderFill    * Fill;
	VertexBuffer  * vertexBuffer;
	IndexBuffer   * indexBuffer;

	Model(Vector3f pos, ShaderFill * fill) :
		numVertices(0),
		numIndices(0),
		Pos(pos),
		Rot(),
		Mat(),
		Fill(fill),
		vertexBuffer(nullptr),
		indexBuffer(nullptr)
	{}

	~Model()
	{
		FreeBuffers();
	}

	Matrix4f& GetMatrix()
	{
		Mat = Matrix4f(Rot);
		Mat = Matrix4f::Translation(Pos) * Mat;
		return Mat;
	}

	void AddVertex(const Vertex& v) { Vertices[numVertices++] = v; }
	void AddIndex(GLushort a) { Indices[numIndices++] = a; }

	void AllocateBuffers()
	{
		vertexBuffer = new VertexBuffer(&Vertices[0], numVertices * sizeof(Vertices[0]));
		indexBuffer = new IndexBuffer(&Indices[0], numIndices * sizeof(Indices[0]));
	}

	void FreeBuffers()
	{
		delete vertexBuffer; vertexBuffer = nullptr;
		delete indexBuffer; indexBuffer = nullptr;
	}

	void AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, DWORD c)
	{
		Vector3f Vert[][2] =
		{
			Vector3f(x1, y2, z1), Vector3f(z1, x1), Vector3f(x2, y2, z1), Vector3f(z1, x2),
			Vector3f(x2, y2, z2), Vector3f(z2, x2), Vector3f(x1, y2, z2), Vector3f(z2, x1),
			Vector3f(x1, y1, z1), Vector3f(z1, x1), Vector3f(x2, y1, z1), Vector3f(z1, x2),
			Vector3f(x2, y1, z2), Vector3f(z2, x2), Vector3f(x1, y1, z2), Vector3f(z2, x1),
			Vector3f(x1, y1, z2), Vector3f(z2, y1), Vector3f(x1, y1, z1), Vector3f(z1, y1),
			Vector3f(x1, y2, z1), Vector3f(z1, y2), Vector3f(x1, y2, z2), Vector3f(z2, y2),
			Vector3f(x2, y1, z2), Vector3f(z2, y1), Vector3f(x2, y1, z1), Vector3f(z1, y1),
			Vector3f(x2, y2, z1), Vector3f(z1, y2), Vector3f(x2, y2, z2), Vector3f(z2, y2),
			Vector3f(x1, y1, z1), Vector3f(x1, y1), Vector3f(x2, y1, z1), Vector3f(x2, y1),
			Vector3f(x2, y2, z1), Vector3f(x2, y2), Vector3f(x1, y2, z1), Vector3f(x1, y2),
			Vector3f(x1, y1, z2), Vector3f(x1, y1), Vector3f(x2, y1, z2), Vector3f(x2, y1),
			Vector3f(x2, y2, z2), Vector3f(x2, y2), Vector3f(x1, y2, z2), Vector3f(x1, y2)
		};

		GLushort CubeIndices[] =
		{
			0, 1, 3, 3, 1, 2,
			5, 4, 6, 6, 4, 7,
			8, 9, 11, 11, 9, 10,
			13, 12, 14, 14, 12, 15,
			16, 17, 19, 19, 17, 18,
			21, 20, 22, 22, 20, 23
		};

		for (int i = 0; i < sizeof(CubeIndices) / sizeof(CubeIndices[0]); ++i)
			AddIndex(CubeIndices[i] + GLushort(numVertices));

		// Generate a quad for each box face
		for (int v = 0; v < 6 * 4; v++)
		{
			// Make vertices, with some token lighting
			Vertex vvv; vvv.Pos = Vert[v][0]; vvv.U = Vert[v][1].x; vvv.V = Vert[v][1].y;
			float dist1 = (vvv.Pos - Vector3f(-2, 4, -2)).Length();
			float dist2 = (vvv.Pos - Vector3f(3, 4, -3)).Length();
			float dist3 = (vvv.Pos - Vector3f(-4, 3, 25)).Length();
			int   bri = rand() % 160;
			float B = ((c >> 16) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
			float G = ((c >> 8) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
			float R = ((c >> 0) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
			vvv.C = (c & 0xff000000) +
				((R > 255 ? 255 : DWORD(R)) << 16) +
				((G > 255 ? 255 : DWORD(G)) << 8) +
				(B > 255 ? 255 : DWORD(B));
			AddVertex(vvv);
		}
	}

	void Render(Matrix4f view, Matrix4f proj)
	{
		Matrix4f combined = proj * view * GetMatrix();

		glUseProgram(Fill->program);
		glUniform1i(glGetUniformLocation(Fill->program, "Texture0"), 0);
		glUniformMatrix4fv(glGetUniformLocation(Fill->program, "matWVP"), 1, GL_TRUE, (FLOAT*)&combined);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Fill->texture->texId);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->buffer);

		GLuint posLoc = glGetAttribLocation(Fill->program, "Position");
		GLuint colorLoc = glGetAttribLocation(Fill->program, "Color");
		GLuint uvLoc = glGetAttribLocation(Fill->program, "TexCoord");

		glEnableVertexAttribArray(posLoc);
		glEnableVertexAttribArray(colorLoc);
		glEnableVertexAttribArray(uvLoc);

		glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, Pos));
		glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, C));
		glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, U));

		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, NULL);

		glDisableVertexAttribArray(posLoc);
		glDisableVertexAttribArray(colorLoc);
		glDisableVertexAttribArray(uvLoc);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glUseProgram(0);
	}
};

//------------------------------------------------------------------------- 
// This MyDots structure was created to deal with the point cloud data, using 
// a similar aproach to what is presented in the Model struct, provided by 
// the original file

struct MyDots
{
	btCollisionShape *sphereShape;
	btDefaultMotionState *sphereMotionState;
	btScalar massRigidBody;
	btVector3 sphereInertia;
	btRigidBody::btRigidBodyConstructionInfo *sphereRigidBodyCI;
	btRigidBody *sphereRigidBody;
	btRigidBody* rigidBodyArray[6 * 25]; //Sphere rigid bodies for each body * joint
	int bodyTrackedBefore[6];

	GLuint vao_position;
	GLuint vao_joints;
	GLuint vbo_position;
	GLuint vbo_joints;
	ShaderFill    * Fill;
	Quatf           Rot;
	Matrix4f        Mat;
	Vector3f        Pos;
	bool mode = false;
	int* bodyTracked = new int[6];
	CameraSpacePoint* headPositions = new CameraSpacePoint[6];

	GLfloat* position = new GLfloat[depth_height*depth_width * 3];//NULL;
	GLubyte* color = new GLubyte[depth_height*depth_width * 3];//NULL;
	int numPoints = depth_height*depth_width;
	int pixelCount;

	float* jointsVertices = NULL;
	RGBQUAD* ColorData = NULL;
	BYTE* BodyIndexBuffer = NULL;
	UINT16* DepthBuffer = NULL;

	MyDots(Vector3f pos)
	{
		Pos = pos;

		for (int i = 0; i < 6; i++)
		{
			rigidBodyArray[i] = 0;
		}

		/*float radio = 0.01;
		sphereShape = new btSphereShape(radio);
		sphereMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
		float massRigidBody = 0.1;
		sphereInertia = btVector3(0, 0, 0);
		sphereShape->calculateLocalInertia(massRigidBody, sphereInertia);
		sphereRigidBodyCI = new btRigidBody::btRigidBodyConstructionInfo(massRigidBody, sphereMotionState, sphereShape, sphereInertia);*/

		for (int i = 0; i < 6 * 25; i++)
		{
			float radio = 0.01;
			sphereShape = new btSphereShape(radio);
			sphereMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 0, 0)));
			float massRigidBody = 0.1;
			sphereInertia = btVector3(0, 0, 0);
			sphereShape->calculateLocalInertia(massRigidBody, sphereInertia);
			sphereRigidBodyCI = new btRigidBody::btRigidBodyConstructionInfo(massRigidBody, sphereMotionState, sphereShape, sphereInertia);
			sphereRigidBody = new btRigidBody(*sphereRigidBodyCI);
			sphereRigidBody->setRestitution(btScalar(0.1));
			sphereRigidBody->setFriction(btScalar(2));
			sphereRigidBody->setCollisionFlags(sphereRigidBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
			sphereRigidBody->setActivationState(DISABLE_DEACTIVATION);
			rigidBodyArray[i] = sphereRigidBody;
		}

		glGenVertexArrays(1, &vao_joints);
		glBindVertexArray(vao_joints);
		glGenBuffers(1, &vbo_joints);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_joints);


		glGenVertexArrays(1, &vao_position);
		glBindVertexArray(vao_position);

		//GLuint vbo;
		glGenBuffers(1, &vbo_position);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_position);
		//glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_position), vertices_position, GL_DYNAMIC_DRAW);  //== moved to updateImage; 

		static const GLchar* VertexShaderSrc =
			"#version 150\n"
			"uniform mat4 matWVP;\n"
			"in vec4 position;\n"
			"in vec3 color;"
			"out vec3 fragmentColor;"
			"void main(){\n"
			"   gl_Position = (matWVP * position);\n"
			"	fragmentColor = color;"
			"}";

		static const GLchar* FragmentShaderSrc =
			"#version 150\n"
			"in vec3 fragmentColor;\n"
			"out vec4 out_color;\n"
			"void main() {\n"
			//"	out_color = vec4(0.1,1.0,0.0,1.0);\n"
			"	out_color = vec4(fragmentColor, 1.0);\n"
			"}";

		GLuint vshader = createShader(VertexShaderSrc, GL_VERTEX_SHADER);
		GLuint fshader = createShader(FragmentShaderSrc, GL_FRAGMENT_SHADER);

		///*************
		//ShaderFill * grid_material;

		static DWORD tex_pixels[256 * 256];
		for (int j = 0; j < 256; ++j)
		{
			for (int i = 0; i < 256; ++i)
			{
				tex_pixels[j * 256 + i] = 0xffffffff;// blank
			}
		}
		TextureBuffer * generated_texture = new TextureBuffer(nullptr, false, false, Sizei(256, 256), 4, (unsigned char *)tex_pixels, 1);

		Fill = new ShaderFill(vshader, fshader, generated_texture);
	}

	Matrix4f& GetMatrix()
	{
		Mat = Matrix4f(Rot);
		Mat = Matrix4f::Translation(Pos) * Mat;
		return Mat;
	}

	void display(Matrix4f view, Matrix4f proj)
	{
		Matrix4f combined = proj * view * GetMatrix();

		//Preparing body vertices to DrawArrays
		glBindVertexArray(vao_position);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_position);

		glUseProgram(Fill->program);
		glUniformMatrix4fv(glGetUniformLocation(Fill->program, "matWVP"), 1, GL_TRUE, (FLOAT*)&combined);

		GLint position_attribute = glGetAttribLocation(Fill->program, "position");
		GLuint color_attribute = glGetAttribLocation(Fill->program, "color");

		glEnableVertexAttribArray(position_attribute);
		glEnableVertexAttribArray(color_attribute);

		//glVertexAttribPointer(position_attribute, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glVertexAttribPointer(position_attribute, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), 0);
		glVertexAttribPointer(color_attribute, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));

		//GLint color_attribute = glGetAttribLocation(Fill->program, "color");
		//glVertexAttribPointer(color_attribute, 3, GL_FLOAT, GL_TRUE, 0, 0);
		//glEnableVertexAttribArray(color_attribute);

		glClear(GL_COLOR_BUFFER_BIT);

		glBindVertexArray(vao_position);
		glEnable(GL_POINT_SMOOTH);
		glPointSize(1);
		glDrawArrays(GL_POINTS, 0, numPoints);

		for (int i = 0; i < BODY_COUNT; i++)
		{
			if (bodyTracked[i] == 1)
			{
				glBindVertexArray(vao_joints);
				glBindBuffer(GL_ARRAY_BUFFER, vbo_joints);

				glUseProgram(Fill->program);
				glUniformMatrix4fv(glGetUniformLocation(Fill->program, "matWVP"), 1, GL_TRUE, (FLOAT*)&combined);

				GLint position_attribute = glGetAttribLocation(Fill->program, "position");
				GLuint color_attribute = glGetAttribLocation(Fill->program, "color");

				glEnableVertexAttribArray(position_attribute);
				glEnableVertexAttribArray(color_attribute);

				//glVertexAttribPointer(position_attribute, 3, GL_FLOAT, GL_FALSE, 0, 0);
				glVertexAttribPointer(position_attribute, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
				glVertexAttribPointer(color_attribute, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

				glBindVertexArray(vao_joints);
				glEnable(GL_POINT_SMOOTH);
				glPointSize(10);
				glDrawArrays(GL_POINTS, 25 * i, 25);

				if (bodyTrackedBefore[i] == 0)
				{
					for (int j = 0; j < 25; j++)
					{
						dynamicsWorld->addRigidBody(rigidBodyArray[j + 25 * i]);
						//rigidBodyArray[j + 25 * 6 * i]->setCollisionFlags(point->getRigidBody()->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);
					}
				}


				btTransform trans;
				for (int j = 0; j < 25; j++)
				{
					float x = jointsVertices[((j * 6) + 0) + JointType_Count*i * 6];
					float y = jointsVertices[((j * 6) + 1) + JointType_Count*i * 6];
					float z = jointsVertices[((j * 6) + 2) + JointType_Count*i * 6];

					rigidBodyArray[j + 25 * i]->getMotionState()->getWorldTransform(trans);
					trans.setOrigin(btVector3(x, y, z));
					rigidBodyArray[j + 25 * i]->getMotionState()->setWorldTransform(trans);
				}

				bodyTrackedBefore[i] = 1;
			}
			else
			{
				if (bodyTrackedBefore[i] == 1)
				{
					for (int j = 0; j < 25; j++)
					{
						dynamicsWorld->removeRigidBody(rigidBodyArray[j + 25 * i]);
					}
				}

				bodyTrackedBefore[i] = 0;
			}
		}

		glDisableVertexAttribArray(position_attribute);
		glDisableVertexAttribArray(color_attribute);
		glUseProgram(0);
	}

	GLuint createShader(const GLchar* src, GLenum shaderType)
	{
		GLuint shader = glCreateShader(shaderType);
		glShaderSource(shader, 1, &src, NULL);
		glCompileShader(shader);

		//Check compilation
		GLint test;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &test);
		if (!test)
		{
			GLchar msg[1024];
			glGetShaderInfoLog(shader, sizeof(msg), 0, msg);
			if (msg[0]) {
				cout << "Shader compilation failed! : " << msg << endl;
			}
		}

		return shader;
	}

	GLuint createProgram(const char* vertexShaderSrc, const char* fragmentShaderSrc)
	{
		GLuint vertexShader = createShader(vertexShaderSrc, GL_VERTEX_SHADER);
		GLuint fragmentShader = createShader(fragmentShaderSrc, GL_FRAGMENT_SHADER);

		GLuint shaderProgram = glCreateProgram();
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		glLinkProgram(shaderProgram);
		glUseProgram(shaderProgram);

		return shaderProgram;
	}

	void updatePoints()
	{
		kinect->GetColorDepthAndBody(ColorData, BodyIndexBuffer, DepthBuffer, jointsVertices, bodyTracked, headPositions);
		pixelCount = 0;

		if (DepthBuffer != NULL)
		{
			delete[] position;
			position = new GLfloat[(depth_height*depth_width * 3 * 2)];

			#pragma omp parallel for schedule(dynamic)
			for (int i = 0; i < depth_height; i += 2)
			{
				for (int j = 0; j < depth_width; j += 1)
				{
					if (mode && bodyTracked)
					{
						if (BodyIndexBuffer[i * depth_width + j] != 0xff)
						{
							DepthSpacePoint depthSpacePoint = { static_cast<float>(j), static_cast<float>(i) };
							UINT16 depth = DepthBuffer[i * depth_width + j];
							ColorSpacePoint colorSpacePoint = { 0.0f, 0.0f };
							kinect->m_pCoordinateMapper->MapDepthPointToColorSpace(depthSpacePoint, depth, &colorSpacePoint);
							int colorX = static_cast<int>(std::floor(colorSpacePoint.X + 0.5f));
							int colorY = static_cast<int>(std::floor(colorSpacePoint.Y + 0.5f));

							if ((0 <= colorX) && (colorX < color_width) && (0 <= colorY) && (colorY < color_height))
							{
								RGBQUAD colorRGB = ColorData[colorY * color_width + colorX];
								CameraSpacePoint cameraSpacePoint = { 0.0f, 0.0f, 0.0f };
								kinect->m_pCoordinateMapper->MapDepthPointToCameraSpace(depthSpacePoint, depth, &cameraSpacePoint);
								position[pixelCount * 6] = cameraSpacePoint.X;
								position[pixelCount * 6 + 1] = cameraSpacePoint.Y;
								position[pixelCount * 6 + 2] = cameraSpacePoint.Z;

								position[pixelCount * 6 + 3] = static_cast<float>(colorRGB.rgbRed) / 255;
								position[pixelCount * 6 + 4] = static_cast<float>(colorRGB.rgbGreen) / 255;
								position[pixelCount * 6 + 5] = static_cast<float>(colorRGB.rgbBlue) / 255;

								pixelCount++;
							}
						}
					}
					else
					{
						DepthSpacePoint depthSpacePoint = { static_cast<float>(j), static_cast<float>(i) };
						UINT16 depth = DepthBuffer[i * depth_width + j];
						ColorSpacePoint colorSpacePoint = { 0.0f, 0.0f };
						kinect->m_pCoordinateMapper->MapDepthPointToColorSpace(depthSpacePoint, depth, &colorSpacePoint);
						int colorX = static_cast<int>(std::floor(colorSpacePoint.X + 0.5f));
						int colorY = static_cast<int>(std::floor(colorSpacePoint.Y + 0.5f));

						if ((0 <= colorX) && (colorX < color_width) && (0 <= colorY) && (colorY < color_height))
						{
							RGBQUAD colorRGB = ColorData[colorY * color_width + colorX];
							CameraSpacePoint cameraSpacePoint = { 0.0f, 0.0f, 0.0f };
							kinect->m_pCoordinateMapper->MapDepthPointToCameraSpace(depthSpacePoint, depth, &cameraSpacePoint);
							position[pixelCount * 6] = cameraSpacePoint.X;
							position[pixelCount * 6 + 1] = cameraSpacePoint.Y;
							position[pixelCount * 6 + 2] = cameraSpacePoint.Z;

							position[pixelCount * 6 + 3] = static_cast<float>(colorRGB.rgbRed) / 255;
							position[pixelCount * 6 + 4] = static_cast<float>(colorRGB.rgbGreen) / 255;
							position[pixelCount * 6 + 5] = static_cast<float>(colorRGB.rgbBlue) / 255;

							pixelCount++;
						}
					}
				}
			}

			glBindVertexArray(vao_position);
			glBindBuffer(GL_ARRAY_BUFFER, vbo_position);
			glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*depth_height*depth_width * 3 * 2, position, GL_STATIC_DRAW);

			glBindVertexArray(vao_joints);
			glBindBuffer(GL_ARRAY_BUFFER, vbo_joints);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float)* JointType_Count * BODY_COUNT * 3 * 2, jointsVertices, GL_STATIC_DRAW);

		}
	}
};
//--------------------------------------------------------------------------
// this boxModel was created to implement a box object that handles the 
// graphics generation and the physical simulation with the Bullet Engine

struct boxModel
{
	struct Vertex
	{
		Vector3f  Pos;
		DWORD     C;
		float     U, V;
	};

	btCollisionShape *boxShape;
	btDefaultMotionState *boxMotionState;
	btScalar massRigidBody;
	btVector3 boxInertia;
	btRigidBody::btRigidBodyConstructionInfo *boxRigidBodyCI;
	btRigidBody *boxRigidBody;

	Vector3f        Pos;
	Quatf           Rot;
	Matrix4f        Mat;
	int             numVertices, numIndices;
	Vertex          Vertices[2000]; // Note fixed maximum
	GLushort        Indices[2000];
	ShaderFill    * Fill;
	VertexBuffer  * vertexBuffer;
	IndexBuffer   * indexBuffer;

	boxModel(Vector3f pos, ShaderFill * fill) :
		numVertices(0),
		numIndices(0),
		Pos(pos),
		Rot(),
		Mat(),
		Fill(fill),
		vertexBuffer(nullptr),
		indexBuffer(nullptr)
	{}

	~boxModel()
	{
		FreeBuffers();
	}

	Matrix4f& GetMatrix()
	{
		Mat = Matrix4f(Rot);
		Mat = Matrix4f::Translation(Pos) * Mat;
		return Mat;
	}

	void AddVertex(const Vertex& v) { Vertices[numVertices++] = v; }
	void AddIndex(GLushort a) { Indices[numIndices++] = a; }

	void AllocateBuffers()
	{
		vertexBuffer = new VertexBuffer(&Vertices[0], numVertices * sizeof(Vertices[0]));
		indexBuffer = new IndexBuffer(&Indices[0], numIndices * sizeof(Indices[0]));
	}

	void FreeBuffers()
	{
		delete vertexBuffer; vertexBuffer = nullptr;
		delete indexBuffer; indexBuffer = nullptr;
	}

	void setupBulletRigidBody(float x1, float y1, float z1, float x2, float y2, float z2)
	{
		btVector3 halfExtents;
		halfExtents.setX(x2);
		halfExtents.setY(y2);
		halfExtents.setZ(z2);
		//halfExtents.setX(0.1);
		//halfExtents.setY(0.1);
		//halfExtents.setZ(0.1);
		boxShape = new btBoxShape(halfExtents);
		boxMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(Pos.x, Pos.y, Pos.z)));
		float massRigidBody = 1;
		boxInertia = btVector3(0, 0, 0);
		boxShape->calculateLocalInertia(massRigidBody, boxInertia);
		boxRigidBodyCI = new btRigidBody::btRigidBodyConstructionInfo(massRigidBody, boxMotionState, boxShape, boxInertia);
		boxRigidBody = new btRigidBody(*boxRigidBodyCI);
		boxRigidBody->setRestitution(btScalar(0.7));
		boxRigidBody->setFriction(btScalar(0.9));
		dynamicsWorld->addRigidBody(boxRigidBody);
	}

	void AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, DWORD c)
	{
		Vector3f Vert[][2] =
		{
			Vector3f(x1, y2, z1), Vector3f(z1, x1), Vector3f(x2, y2, z1), Vector3f(z1, x2),
			Vector3f(x2, y2, z2), Vector3f(z2, x2), Vector3f(x1, y2, z2), Vector3f(z2, x1),
			Vector3f(x1, y1, z1), Vector3f(z1, x1), Vector3f(x2, y1, z1), Vector3f(z1, x2),
			Vector3f(x2, y1, z2), Vector3f(z2, x2), Vector3f(x1, y1, z2), Vector3f(z2, x1),
			Vector3f(x1, y1, z2), Vector3f(z2, y1), Vector3f(x1, y1, z1), Vector3f(z1, y1),
			Vector3f(x1, y2, z1), Vector3f(z1, y2), Vector3f(x1, y2, z2), Vector3f(z2, y2),
			Vector3f(x2, y1, z2), Vector3f(z2, y1), Vector3f(x2, y1, z1), Vector3f(z1, y1),
			Vector3f(x2, y2, z1), Vector3f(z1, y2), Vector3f(x2, y2, z2), Vector3f(z2, y2),
			Vector3f(x1, y1, z1), Vector3f(x1, y1), Vector3f(x2, y1, z1), Vector3f(x2, y1),
			Vector3f(x2, y2, z1), Vector3f(x2, y2), Vector3f(x1, y2, z1), Vector3f(x1, y2),
			Vector3f(x1, y1, z2), Vector3f(x1, y1), Vector3f(x2, y1, z2), Vector3f(x2, y1),
			Vector3f(x2, y2, z2), Vector3f(x2, y2), Vector3f(x1, y2, z2), Vector3f(x1, y2)
		};

		GLushort CubeIndices[] =
		{
			0, 1, 3, 3, 1, 2,
			5, 4, 6, 6, 4, 7,
			8, 9, 11, 11, 9, 10,
			13, 12, 14, 14, 12, 15,
			16, 17, 19, 19, 17, 18,
			21, 20, 22, 22, 20, 23
		};

		for (int i = 0; i < sizeof(CubeIndices) / sizeof(CubeIndices[0]); ++i)
			AddIndex(CubeIndices[i] + GLushort(numVertices));

		// Generate a quad for each box face
		for (int v = 0; v < 6 * 4; v++)
		{
			// Make vertices, with some token lighting
			Vertex vvv; vvv.Pos = Vert[v][0]; vvv.U = Vert[v][1].x; vvv.V = Vert[v][1].y;
			float dist1 = (vvv.Pos - Vector3f(-2, 4, -2)).Length();
			float dist2 = (vvv.Pos - Vector3f(3, 4, -3)).Length();
			float dist3 = (vvv.Pos - Vector3f(-4, 3, 25)).Length();
			int   bri = rand() % 160;
			float B = ((c >> 16) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
			float G = ((c >> 8) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
			float R = ((c >> 0) & 0xff) * (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
			vvv.C = (c & 0xff000000) +
				((R > 255 ? 255 : DWORD(R)) << 16) +
				((G > 255 ? 255 : DWORD(G)) << 8) +
				(B > 255 ? 255 : DWORD(B));
			AddVertex(vvv);
		}

		setupBulletRigidBody(x1, y1, z1, x2, y2, z2);
	}

	void Render(Matrix4f view, Matrix4f proj)
	{
		Matrix4f combined = proj * view * GetMatrix();

		glUseProgram(Fill->program);
		glUniform1i(glGetUniformLocation(Fill->program, "Texture0"), 0);
		glUniformMatrix4fv(glGetUniformLocation(Fill->program, "matWVP"), 1, GL_TRUE, (FLOAT*)&combined);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, Fill->texture->texId);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->buffer);

		GLuint posLoc = glGetAttribLocation(Fill->program, "Position");
		GLuint colorLoc = glGetAttribLocation(Fill->program, "Color");
		GLuint uvLoc = glGetAttribLocation(Fill->program, "TexCoord");

		glEnableVertexAttribArray(posLoc);
		glEnableVertexAttribArray(colorLoc);
		glEnableVertexAttribArray(uvLoc);

		glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, Pos));
		glVertexAttribPointer(colorLoc, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, C));
		glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, U));

		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, NULL);

		glDisableVertexAttribArray(posLoc);
		glDisableVertexAttribArray(colorLoc);
		glDisableVertexAttribArray(uvLoc);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glUseProgram(0);
	}
};

//---------------------------------------------------------------------------

// this SphereModel was created to implement a sphere object that handles the 
// graphics generation and the physical simulation with the Bullet Engine

struct SphereModel
{
	struct Vertex
	{
		Vector3f  Pos;
		DWORD     C;
		float     U, V;
	};

	//Bullet simulation Objects
	btCollisionShape *sphereShape;
	btDefaultMotionState *sphereMotionState;
	btScalar massRigidBody;
	btVector3 sphereInertia;
	btRigidBody::btRigidBodyConstructionInfo *sphereRigidBodyCI;
	btRigidBody *sphereRigidBody;

	Vector3f        Pos;
	Quatf           Rot;
	Matrix4f        Mat;
	int             numVertices, numIndices;
	Vertex          Vertices[2000]; // Note fixed maximum
	GLushort        Indices[2000];
	ShaderFill    * Fill;
	VertexBuffer  * vertexBuffer;
	IndexBuffer   * indexBuffer;
	float radio;


	SphereModel(Vector3f pos, ShaderFill * fill) :
		numVertices(0),
		numIndices(0),
		Pos(pos),
		Rot(),
		Mat(),
		Fill(fill),
		vertexBuffer(nullptr),
		indexBuffer(nullptr)
	{}

	~SphereModel()
	{
		FreeBuffers();
	}

	Matrix4f& GetMatrix()
	{
		Mat = Matrix4f(Rot);
		Mat = Matrix4f::Translation(Pos) * Mat;
		return Mat;
	}

	void AddVertex(const Vertex& v) { Vertices[numVertices++] = v; }
	void AddIndex(GLushort a) { Indices[numIndices++] = a; }

	void AllocateBuffers()
	{
		vertexBuffer = new VertexBuffer(&Vertices[0], numVertices * sizeof(Vertices[0]));
		indexBuffer = new IndexBuffer(&Indices[0], numIndices * sizeof(Indices[0]));
	}

	void FreeBuffers()
	{
		delete vertexBuffer; vertexBuffer = nullptr;
		delete indexBuffer; indexBuffer = nullptr;
	}

	void setupBulletRigidBody()
	{
		sphereShape = new btSphereShape(radio);
		sphereMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(Pos.x, Pos.y, Pos.z)));
		float massRigidBody = 1;
		sphereInertia = btVector3(0, 0, 0);
		sphereShape->calculateLocalInertia(massRigidBody, sphereInertia);
		sphereRigidBodyCI = new btRigidBody::btRigidBodyConstructionInfo(massRigidBody, sphereMotionState, sphereShape, sphereInertia);
		sphereRigidBody = new btRigidBody(*sphereRigidBodyCI);
		sphereRigidBody->setRestitution(btScalar(0.9));
		sphereRigidBody->setFriction(btScalar(0.9));
		dynamicsWorld->addRigidBody(sphereRigidBody);
	}

	void AddSolidSphere(float r, DWORD c)
	{
		radio = r;
		int npointsh = 18, npointsv = 18;

		for (int i = 0; i < npointsh; i++)
		{
			for (int j = 0; j <= npointsv; j++)
			{
				float phi = i*((2 * 3.1415) / npointsv);
				float theta = j*(3.1415 / npointsh);

				Vertex v;
				v.Pos.x = r*cos(phi)*sin(theta);
				v.Pos.y = r*sin(phi)*sin(theta);
				v.Pos.z = r*cos(theta);

				//v.C = ((v.Pos.x * 255 > 255 ? 255 : DWORD(v.Pos.x * 255)) << 16) +
				//((v.Pos.y * 255 > 255 ? 255 : DWORD(v.Pos.y * 255)) << 8) +
				//(v.Pos.z * 255 > 255 ? 255 : DWORD(v.Pos.z * 255));

				AddVertex(v);
			}
		}

		int x = 1;
		for (int i = 0; i < npointsv*npointsh + npointsv; i++)
		{
			if (i == x*npointsv + (x - 1)){
				x++;
				continue;
			}

			if (i + npointsv + 1 > npointsv*npointsh + npointsv)
			{
				AddIndex(i);
				AddIndex((i - npointsv*npointsh) + 1);
				AddIndex(i + 1);

				AddIndex(i);
				AddIndex((i - npointsv*npointsh));
				AddIndex((i - npointsv*npointsh) + 1);
			}
			else
			{
				AddIndex(i);
				AddIndex(i + npointsv + 2);
				AddIndex(i + 1);

				AddIndex(i);
				AddIndex(i + npointsv + 1);
				AddIndex(i + npointsv + 2);
			}

		}

		setupBulletRigidBody();
	}

	void Render(Matrix4f view, Matrix4f proj)
	{
		Matrix4f combined = proj * view * GetMatrix();

		glUseProgram(Fill->program);
		glUniform1i(glGetUniformLocation(Fill->program, "Texture0"), 0);
		glUniformMatrix4fv(glGetUniformLocation(Fill->program, "matWVP"), 1, GL_TRUE, (FLOAT*)&combined);

		glActiveTexture(GL_TEXTURE0);
		//glBindTexture(GL_TEXTURE_2D, Fill->texture->texId);

		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer->buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->buffer);

		GLuint posLoc = glGetAttribLocation(Fill->program, "position");
		GLuint colorLoc = glGetAttribLocation(Fill->program, "color");
		GLuint uvLoc = glGetAttribLocation(Fill->program, "TexCoord");

		glEnableVertexAttribArray(posLoc);
		glEnableVertexAttribArray(colorLoc);
		glEnableVertexAttribArray(uvLoc);

		glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, Pos));
		glVertexAttribPointer(colorLoc, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, Pos));
		glVertexAttribPointer(uvLoc, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)OVR_OFFSETOF(Vertex, U));

		glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, NULL);
		//glEnable(GL_POINT_SMOOTH);
		//glPointSize(5);
		//glDrawArrays(GL_POINTS, 0, numVertices);

		glDisableVertexAttribArray(posLoc);
		glDisableVertexAttribArray(colorLoc);
		glDisableVertexAttribArray(uvLoc);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		//glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glUseProgram(0);
	}
};

//---------------------------------------------------------------------------
// The Scene structure was modified to implement the BulletEngine simulations
// and render the new implemented structs

struct Scene
{
	bool resetBox = false;
	ShaderFill * grid_material[4];
	int     numModels;
	Model * Models[10];
	int     numBoxModels;
	boxModel * boxModels[10];
	MyDots* dotsTest;
	int framecount = 0;
	SphereModel* sphereModel;
	SphereModel* sphereModel2;

	void    Add(Model * n)
	{
		Models[numModels++] = n;
	}

	void AddBox(boxModel * n)
	{
		boxModels[numBoxModels++] = n;
	}

	void Render(Matrix4f view, Matrix4f proj)
	{
		// This part resets the box object in case it has fallen out of reach from the user
		if (resetBox)
		{
			dynamicsWorld->removeRigidBody(boxModels[0]->boxRigidBody);
			boxModel *b;
			DWORD c = 0xff105020;
			float hx = 0.1, hy = 0.7, hz = 0.1;
			b = new boxModel(Vector3f(0, -0.9, 0.6), grid_material[3]);  // Walls
			b->AddSolidColorBox(-hx, -hy, -hz, hx, hy, hz, c); // First Box
			b->AllocateBuffers();
			boxModels[0] = b;
			resetBox = false;
		}

		dynamicsWorld->stepSimulation(1 / 50.f, 1000);
		btTransform trans;

		//updates data points and renders the point cloud
		dotsTest->updatePoints();
		dotsTest->display(view, proj);

		//Renders default models
		for (int i = 0; i < numModels; ++i)
			Models[i]->Render(view, proj);

		//Updates rotation and position of each box model
		for (int i = 0; i < numBoxModels; ++i)
		{
			boxModels[i]->boxRigidBody->getMotionState()->getWorldTransform(trans);
			boxModels[i]->Pos = Vector3f(trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ());
			boxModels[i]->Rot = Quatf(trans.getRotation().getX(), trans.getRotation().getY(), trans.getRotation().getZ(), trans.getRotation().getW());
			boxModels[i]->Render(view, proj);
		}

		//updates rotation and position for sphere
		sphereModel2->sphereRigidBody->getMotionState()->getWorldTransform(trans);
		sphereModel2->Pos = Vector3f(trans.getOrigin().getX(), trans.getOrigin().getY(), trans.getOrigin().getZ());
		sphereModel2->Rot = Quatf(trans.getRotation().getX(), trans.getRotation().getY(), trans.getRotation().getZ(), trans.getRotation().getW());
		sphereModel2->Render(view, proj);
	}

	GLuint CreateShader(GLenum type, const GLchar* src)
	{
		GLuint shader = glCreateShader(type);

		glShaderSource(shader, 1, &src, NULL);
		glCompileShader(shader);

		GLint r;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &r);
		if (!r)
		{
			GLchar msg[1024];
			glGetShaderInfoLog(shader, sizeof(msg), 0, msg);
			if (msg[0]) {
				OVR_DEBUG_LOG(("Compiling shader failed: %s\n", msg));
			}
			return 0;
		}

		return shader;
	}

	void Init(int includeIntensiveGPUobject)
	{
		kinect = new KinectHandler();
		kinect->KinectInit();

		dotsTest = new MyDots(Vector3f(0, 0, 0));

		static const GLchar* VertexShaderSrc =
			"#version 150\n"
			"uniform mat4 matWVP;\n"
			"in      vec4 Position;\n"
			"in      vec4 Color;\n"
			"in      vec2 TexCoord;\n"
			"out     vec2 oTexCoord;\n"
			"out     vec4 oColor;\n"
			"void main()\n"
			"{\n"
			"   gl_Position = (matWVP * Position);\n"
			"   oTexCoord   = TexCoord;\n"
			"   oColor.rgb  = pow(Color.rgb, vec3(2.2));\n"   // convert from sRGB to linear
			"   oColor.a    = Color.a;\n"
			"}\n";

		static const char* FragmentShaderSrc =
			"#version 150\n"
			"uniform sampler2D Texture0;\n"
			"in      vec4      oColor;\n"
			"in      vec2      oTexCoord;\n"
			"out     vec4      FragColor;\n"
			"void main()\n"
			"{\n"
			"   FragColor = oColor * texture2D(Texture0, oTexCoord);\n"
			"}\n";

		GLuint    vshader = CreateShader(GL_VERTEX_SHADER, VertexShaderSrc);
		GLuint    fshader = CreateShader(GL_FRAGMENT_SHADER, FragmentShaderSrc);

		// Make textures
		for (int k = 0; k < 4; ++k)
		{
			static DWORD tex_pixels[256 * 256];
			for (int j = 0; j < 256; ++j)
			{
				for (int i = 0; i < 256; ++i)
				{
					if (k == 0) tex_pixels[j * 256 + i] = (((i >> 7) ^ (j >> 7)) & 1) ? 0xffb4b4b4 : 0xff505050;// floor
					if (k == 1) tex_pixels[j * 256 + i] = (((j / 4 & 15) == 0) || (((i / 4 & 15) == 0) && ((((i / 4 & 31) == 0) ^ ((j / 4 >> 4) & 1)) == 0)))
						? 0xff3c3c3c : 0xffb4b4b4;// wall
					if (k == 2) tex_pixels[j * 256 + i] = (i / 4 == 0 || j / 4 == 0) ? 0xff505050 : 0xffb4b4b4;// ceiling
					if (k == 3) tex_pixels[j * 256 + i] = 0xffffffff;// blank
				}
			}
			TextureBuffer * generated_texture = new TextureBuffer(nullptr, false, false, Sizei(256, 256), 4, (unsigned char *)tex_pixels, 1);
			grid_material[k] = new ShaderFill(vshader, fshader, generated_texture);
		}

		glDeleteShader(vshader);
		glDeleteShader(fshader);

		//=============================================================
		btBroadphaseInterface* broadphase = new btDbvtBroadphase();
		btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
		btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);
		btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;
		dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
		dynamicsWorld->setGravity(btVector3(0, -9.8, 0));

		//Initializes static object that represents the ground in the dynamicsWorld
		btCollisionShape* groundShape = new btStaticPlaneShape(btVector3(0, 1, 0), 0);
		btDefaultMotionState* groundMotionState = new btDefaultMotionState(btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, -1, 0)));
		btRigidBody::btRigidBodyConstructionInfo groundRigidBodyCI(0, groundMotionState, groundShape, btVector3(0, 0, 0));
		btRigidBody* groundRigidBody = new btRigidBody(groundRigidBodyCI);
		groundRigidBody->setRestitution(btScalar(0.8));
		groundRigidBody->setFriction(1);
		dynamicsWorld->addRigidBody(groundRigidBody);

		static const GLchar* mVertexShaderSrc =
			"#version 150\n"
			"uniform mat4	matWVP;\n"
			"in		vec4	position;\n"
			"in		vec4	color;"
			"out	vec4	fragmentColor;"
			"void main(){\n"
			"   gl_Position		=	(matWVP * position);\n"
			"	fragmentColor	=	color;"
			"}";

		static const GLchar* mFragmentShaderSrc =
			"#version 150\n"
			"in		vec4	fragmentColor;\n"
			"out	vec4	out_color;\n"
			"void main() {\n"
			//"	out_color		=	vec4(fragmentColor, 1.0);\n"
			"	out_color		=	fragmentColor;\n"
			"}";

		GLuint    mvshader = CreateShader(GL_VERTEX_SHADER, mVertexShaderSrc);
		GLuint    mfshader = CreateShader(GL_FRAGMENT_SHADER, mFragmentShaderSrc);
		ShaderFill* myShader = new ShaderFill(mvshader, mfshader, NULL);

		sphereModel2 = new SphereModel(Vector3f(0.35, 2, 0.8), myShader);
		sphereModel2->AddSolidSphere(0.15f, 0xff202050);
		sphereModel2->AllocateBuffers();

		//=============================================================

		Model * m;

		m = new Model(Vector3f(0, 0, 0), grid_material[3]);  // Walls
		DWORD c = 0xff202020; // Walls color
		m->AddSolidColorBox(-10.1f, -1.0f, -20.0f, -10.0f, 4.0f, 20.0f, c); // Left Wall
		m->AddSolidColorBox(-10.0f, -1.1f, -20.1f, 10.0f, 4.0f, -20.0f, c); // Back Wall
		m->AddSolidColorBox(10.0f, -1.1f, -20.0f, 10.1f, 4.0f, 20.0f, c); // Right Wall
		m->AddSolidColorBox(-10.0f, -1.1f, -20.0f, 10.0f, -1.0f, 20.1f, c); // Main floor
		m->AddSolidColorBox(-10.0f, 4.0f, -20.0f, 10.0f, 4.1f, 20.1f, c); //ceiling
		m->AddSolidColorBox(-10.0f, -1.1f, 20.1f, 10.0f, 4.0f, 20.0f, c); // Front Wall
		m->AllocateBuffers();
		Add(m);

		boxModel *b;
		c = 0xff105020;
		float hx = 0.1, hy = 0.7, hz = 0.1;
		b = new boxModel(Vector3f(0, -0.9, 0.6), grid_material[3]);
		b->AddSolidColorBox(-hx, -hy, -hz, hx, hy, hz, c); // First Box
		b->AllocateBuffers();
		AddBox(b);

		// In order to see how to add more objects from the Model Structure
		// refer to the original project available in the Oculus Rift SDK 0.8

	}

	Scene() : numModels(0), numBoxModels(0) {}
	Scene(bool includeIntensiveGPUobject) :
		numModels(0), numBoxModels(0)
	{
		Init(includeIntensiveGPUobject);
	}
	void Release()
	{
		while (numModels-- > 0)
			delete Models[numModels];

		while (numBoxModels-- > 0)
			delete boxModels[numBoxModels];
	}
	~Scene()
	{
		Release();
	}
};

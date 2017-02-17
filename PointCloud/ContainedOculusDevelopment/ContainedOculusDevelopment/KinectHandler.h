#pragma once
#include <Kinect.h>
#include <omp.h>

typedef struct PointCloud
{
	RGBQUAD color;
	float X;
	float Y;
	float Z;
} PointCloud;

class KinectHandler
{
public:
	static const int cDepthWidth = 512;
	static const int cDepthHeight = 424;
	static const int cColorWidth = 1920;
	static const int cColorHeight = 1080;

	//Coordinate Mapper
	ICoordinateMapper*			m_pCoordinateMapper;

	//Constructor
	KinectHandler();

	//Destructor
	~KinectHandler();

	//Initialize kinect device
	HRESULT KinectInit();
	HRESULT GetColorData(RGBQUAD*& dest);
	HRESULT GetDepthImageData(RGBQUAD*&  dest);
	HRESULT GetColorDepthAndBody(RGBQUAD* &color, BYTE* &bodyIndex, UINT16*& depthBuffer, float*& bodies, int*& bodyTracked, CameraSpacePoint*& headPositions);
	HRESULT GetColorAndDepth(RGBQUAD*&  color, RGBQUAD*& depth);

private:
	// Current Kinect
	IKinectSensor*				m_pKinectSensor;

	// Depth reader
	IDepthFrameReader*			m_pDepthFrameReader;
	IColorFrameReader*			m_pColorFrameReader;
	IMultiSourceFrameReader*	m_pMultiFrameReader;

	RGBQUAD*					m_pColorRGBX;
	RGBQUAD*					m_pDepthRGBX;
	UINT16*						m_pDepthRawBuffer;
	BYTE*						m_pBodyIndexBuffer;
	float*						m_pJointVertices;


	// Safe release for interfaces
	template<class Interface>
	inline void SafeRelease(Interface *& pInterfaceToRelease)
	{
		if (pInterfaceToRelease != NULL)
		{
			pInterfaceToRelease->Release();
			pInterfaceToRelease = NULL;
		}
	}

};
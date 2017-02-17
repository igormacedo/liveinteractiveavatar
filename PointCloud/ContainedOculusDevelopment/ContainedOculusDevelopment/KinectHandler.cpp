#include "KinectHandler.h"
#include <iostream>

using namespace std;

KinectHandler::KinectHandler() :
m_pColorFrameReader(NULL),
m_pDepthFrameReader(NULL),
m_pKinectSensor(NULL),
m_pMultiFrameReader(NULL),
m_pColorRGBX(NULL),
m_pDepthRGBX(NULL)
{
	// create heap storage for color pixel data in RGBX format
	m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];
	m_pDepthRGBX = new RGBQUAD[cDepthWidth * cDepthHeight];
	m_pDepthRawBuffer = new UINT16[cDepthWidth * cDepthHeight];
	m_pBodyIndexBuffer = new BYTE[cDepthWidth * cDepthHeight];
	m_pJointVertices = new float[JointType_Count * BODY_COUNT * 3 * 2];
}

KinectHandler::~KinectHandler()
{
	if (m_pColorRGBX)
	{
		delete[] m_pColorRGBX;
		m_pColorRGBX = NULL;
	}

	if (m_pDepthRGBX)
	{
		delete[] m_pDepthRGBX;
		m_pDepthRGBX = NULL;
	}

	// close the Kinect Sensor
	if (m_pKinectSensor)
	{
		m_pKinectSensor->Close();
	}

	SafeRelease(m_pKinectSensor);
}

HRESULT KinectHandler::KinectInit()
{
	HRESULT hr;
	hr = GetDefaultKinectSensor(&m_pKinectSensor);
	if (FAILED(hr)){ return hr; }

	if (m_pKinectSensor)
	{
		//Initialize the Kinect and get depth and color reader;
		hr = m_pKinectSensor->Open();

		if (SUCCEEDED(hr))
		{
			hr = m_pKinectSensor->OpenMultiSourceFrameReader(FrameSourceTypes_Color | FrameSourceTypes_Depth | FrameSourceTypes_BodyIndex | FrameSourceTypes_Body, &m_pMultiFrameReader);
		}
	}

	if (!m_pKinectSensor || FAILED(hr))
	{
		cout << "Get Kinect falhou" << endl;
		return E_FAIL;
	}

	hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);
	if (FAILED(hr)){
		std::cerr << "Error : IKinectSensor::get_CoordinateMapper()" << std::endl;
		return -1;
	}

	return hr;
}

HRESULT KinectHandler::GetColorData(RGBQUAD* &dest)
{
	if (!m_pMultiFrameReader)
	{
		cout << "No frame reader!" << endl;
		return E_FAIL;
	}

	IColorFrame* pColorFrame = NULL;
	IMultiSourceFrame* pMultiSourceFrame = NULL;
	HRESULT hr = m_pMultiFrameReader->AcquireLatestFrame(&pMultiSourceFrame);

	if (SUCCEEDED(hr))
	{
		IColorFrameReference* pColorFrameReference = NULL;
		hr = pMultiSourceFrame->get_ColorFrameReference(&pColorFrameReference);

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameReference->AcquireFrame(&pColorFrame);
		}

		SafeRelease(pColorFrameReference);
	}

	if (SUCCEEDED(hr))
	{
		INT64 nTime = 0;
		IFrameDescription* pColorFrameDescription = NULL;
		int nColorWidth = 0;
		int nColorHeight = 0;
		ColorImageFormat imageFormat = ColorImageFormat_None;
		UINT nColorBufferSize = 0;
		RGBQUAD *pColorBuffer = NULL;

		hr = pColorFrame->get_RelativeTime(&nTime);

		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_FrameDescription(&pColorFrameDescription);
		}
		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Width(&nColorWidth);
		}
		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Height(&nColorHeight);
		}
		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
		}
		if (SUCCEEDED(hr))
		{
			if (imageFormat == ColorImageFormat_Bgra)
			{
				cout << "Acessando o raw format!!!" << endl;
				hr = pColorFrame->AccessRawUnderlyingBuffer(&nColorBufferSize, reinterpret_cast<BYTE**>(&pColorBuffer));
			}
			else if (m_pColorRGBX)
			{
				cout << "Acessando o convert format" << endl;
				pColorBuffer = m_pColorRGBX;
				nColorBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
				hr = pColorFrame->CopyConvertedFrameDataToArray(nColorBufferSize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Bgra);
			}
			else
			{
				cout << "FAILED" << endl;
				hr = E_FAIL;
			}
		}

		if (SUCCEEDED(hr))
		{
			dest = pColorBuffer;
		}
	}
	else
	{
		cout << "Acquire last frame FAILED " << endl;
		hr = E_FAIL;
		SafeRelease(pColorFrame);
		return hr;
	}

	SafeRelease(pColorFrame);
	SafeRelease(pMultiSourceFrame);
	return hr;
}

HRESULT KinectHandler::GetDepthImageData(RGBQUAD* &dest)
{
	if (!m_pMultiFrameReader)
	{
		cout << "No frame reader!" << endl;
		return E_FAIL;
	}

	IDepthFrame* pDepthFrame = NULL;
	IMultiSourceFrame* pMultiSourceFrame = NULL;
	HRESULT hr = m_pMultiFrameReader->AcquireLatestFrame(&pMultiSourceFrame);

	if (SUCCEEDED(hr))
	{
		IDepthFrameReference* pDepthFrameReference = NULL;
		hr = pMultiSourceFrame->get_DepthFrameReference(&pDepthFrameReference);

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameReference->AcquireFrame(&pDepthFrame);
		}

		SafeRelease(pDepthFrameReference);
	}

	if (SUCCEEDED(hr))
	{
		INT64 nTime = 0;
		IFrameDescription* pDepthFrameDescription = NULL;
		int nDepthWidth = 0;
		int nDepthHeight = 0;
		USHORT nDepthMinReliableDistance = 0;
		USHORT nDepthMaxDistance = 0;
		UINT nDepthBufferSize = 0;
		UINT16 *pDepthBuffer = NULL;

		hr = pDepthFrame->get_RelativeTime(&nTime);

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);
		}
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Width(&nDepthWidth);
		}
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Height(&nDepthHeight);
		}
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
		}
		if (SUCCEEDED(hr))
		{
			// In order to see the full range of depth (including the less reliable far field depth)
			// we are setting nDepthMaxDistance to the extreme potential depth threshold
			nDepthMaxDistance = USHRT_MAX;

			// Note:  If you wish to filter by reliable depth distance, uncomment the following line.
			//// hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &pDepthBuffer);
		}
		if (SUCCEEDED(hr))
		{
			//RGBQUAD* pRGBX = new RGBQUAD[cDepthWidth * cDepthHeight];


			// end pixel is start + width*height - 1
			cout << "w:" << nDepthWidth << " h:" << nDepthHeight << endl;
			cout << "buffersize:" << nDepthBufferSize << endl;

			const UINT16* pBufferEnd = pDepthBuffer + (nDepthWidth * nDepthHeight);
			RGBQUAD* auxiliar = m_pDepthRGBX;
			//const UINT16* pBufferEnd = pDepthBuffer + (640 * 480);

			cout << "bufferLocation:" << pDepthBuffer << endl;
			cout << "bufferend:" << pBufferEnd << endl;
			int counter = 0;

			while (pDepthBuffer < pBufferEnd)
			{
				//cout << "now:" << pDepthBuffer << " end:" << pBufferEnd << endl;
				USHORT depth = *pDepthBuffer;
				//cout << "now:" << pDepthBuffer << " end:" << pBufferEnd << endl;

				// To convert to a byte, we're discarding the most-significant
				// rather than least-significant bits.
				// We're preserving detail, although the intensity will "wrap."
				// Values outside the reliable depth range are mapped to 0 (black).

				// Note: Using conditionals in this loop could degrade performance.
				// Consider using a lookup table instead when writing production code.
				BYTE intensity = static_cast<BYTE>((depth >= nDepthMinReliableDistance) && (depth <= nDepthMaxDistance) ? (depth % 256) : 0);
				auxiliar->rgbBlue = intensity;
				auxiliar->rgbGreen = intensity;
				auxiliar->rgbRed = intensity;
				auxiliar->rgbReserved = (BYTE)255;

				counter++;

				++auxiliar;
				++pDepthBuffer;
			}

			dest = m_pDepthRGBX;
			cout << "total struct:" << counter << endl;
		}

		SafeRelease(pDepthFrameDescription);
	}
	else
	{
		cout << "Acquire last frame FAILED " << endl;
		hr = E_FAIL;
		SafeRelease(pDepthFrame);
		return hr;
	}

	SafeRelease(pDepthFrame);
	SafeRelease(pMultiSourceFrame);
	return hr;
}

HRESULT KinectHandler::GetColorAndDepth(RGBQUAD* &color, RGBQUAD* &depth)
{
	if (!m_pMultiFrameReader)
	{
		cout << "No frame reader!" << endl;
		return E_FAIL;
	}

	IColorFrame* pColorFrame = NULL;
	IDepthFrame* pDepthFrame = NULL;
	IMultiSourceFrame* pMultiSourceFrame = NULL;
	HRESULT hr = m_pMultiFrameReader->AcquireLatestFrame(&pMultiSourceFrame);

	if (SUCCEEDED(hr))
	{
		IColorFrameReference* pColorFrameReference = NULL;
		hr = pMultiSourceFrame->get_ColorFrameReference(&pColorFrameReference);

		if (SUCCEEDED(hr))
		{
			hr = pColorFrameReference->AcquireFrame(&pColorFrame);
		}

		IDepthFrameReference* pDepthFrameReference = NULL;
		hr = pMultiSourceFrame->get_DepthFrameReference(&pDepthFrameReference);

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameReference->AcquireFrame(&pDepthFrame);
		}

		SafeRelease(pColorFrameReference);
		SafeRelease(pDepthFrameReference);
	}

	if (SUCCEEDED(hr) && pColorFrame != NULL && pDepthFrame != NULL)
	{
		INT64 nTime = 0;
		IFrameDescription* pColorFrameDescription = NULL;
		int nColorWidth = 0;
		int nColorHeight = 0;
		ColorImageFormat imageFormat = ColorImageFormat_None;
		UINT nColorBufferSize = 0;
		RGBQUAD *pColorBuffer = NULL;

		hr = pColorFrame->get_RelativeTime(&nTime);

		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_FrameDescription(&pColorFrameDescription);
		}
		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Width(&nColorWidth);
		}
		if (SUCCEEDED(hr))
		{
			hr = pColorFrameDescription->get_Height(&nColorHeight);
		}
		if (SUCCEEDED(hr))
		{
			hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
		}
		if (SUCCEEDED(hr))
		{
			if (imageFormat == ColorImageFormat_Bgra)
			{
				hr = pColorFrame->AccessRawUnderlyingBuffer(&nColorBufferSize, reinterpret_cast<BYTE**>(&pColorBuffer));
			}
			else if (m_pColorRGBX)
			{
				pColorBuffer = m_pColorRGBX;
				nColorBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
				hr = pColorFrame->CopyConvertedFrameDataToArray(nColorBufferSize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Bgra);
			}
			else
			{
				cout << "FAILED" << endl;
				hr = E_FAIL;
			}
		}

		if (SUCCEEDED(hr))
		{
			color = pColorBuffer;
		}


		///===========================================////


		nTime = 0;
		IFrameDescription* pDepthFrameDescription = NULL;
		int nDepthWidth = 0;
		int nDepthHeight = 0;
		USHORT nDepthMinReliableDistance = 0;
		USHORT nDepthMaxDistance = 0;
		UINT nDepthBufferSize = 0;
		UINT16 *pDepthBuffer = NULL;

		hr = pDepthFrame->get_RelativeTime(&nTime);

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);
		}
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Width(&nDepthWidth);
		}
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameDescription->get_Height(&nDepthHeight);
		}
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
		}
		if (SUCCEEDED(hr))
		{
			// In order to see the full range of depth (including the less reliable far field depth)
			// we are setting nDepthMaxDistance to the extreme potential depth threshold
			nDepthMaxDistance = USHRT_MAX;

			// Note:  If you wish to filter by reliable depth distance, uncomment the following line.
			//// hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
		}

		if (SUCCEEDED(hr))
		{
			hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &pDepthBuffer);
		}
		if (SUCCEEDED(hr))
		{
			//RGBQUAD* pRGBX = new RGBQUAD[cDepthWidth * cDepthHeight];


			// end pixel is start + width*height - 1

			const UINT16* pBufferEnd = pDepthBuffer + (nDepthWidth * nDepthHeight);
			RGBQUAD* auxiliar = m_pDepthRGBX;
			//const UINT16* pBufferEnd = pDepthBuffer + (640 * 480);
			int counter = 0;

			while (pDepthBuffer < pBufferEnd)
			{
				//cout << "now:" << pDepthBuffer << " end:" << pBufferEnd << endl;
				USHORT depth = *pDepthBuffer;
				//cout << "now:" << pDepthBuffer << " end:" << pBufferEnd << endl;

				// To convert to a byte, we're discarding the most-significant
				// rather than least-significant bits.
				// We're preserving detail, although the intensity will "wrap."
				// Values outside the reliable depth range are mapped to 0 (black).

				// Note: Using conditionals in this loop could degrade performance.
				// Consider using a lookup table instead when writing production code.
				//BYTE intensity = static_cast<BYTE>((depth >= nDepthMinReliableDistance) && (depth <= nDepthMaxDistance) ? (depth % 256) : 0);
				BYTE intensity = static_cast<BYTE>((depth >= nDepthMinReliableDistance) && (depth <= nDepthMaxDistance) ? ((depth - nDepthMinReliableDistance) * (0 - 255) / (nDepthMaxDistance / 80 - nDepthMinReliableDistance) + 255) : 0);
				auxiliar->rgbBlue = intensity;
				auxiliar->rgbGreen = intensity;
				auxiliar->rgbRed = intensity;
				auxiliar->rgbReserved = (BYTE)255;

				counter++;

				++auxiliar;
				++pDepthBuffer;
			}

			depth = m_pDepthRGBX;
		}

		SafeRelease(pDepthFrameDescription);

	}
	else
	{
		cout << "Acquire last frame FAILED " << endl;
		hr = E_FAIL;
		SafeRelease(pColorFrame);
		SafeRelease(pDepthFrame);
		return hr;
	}

	SafeRelease(pColorFrame);
	SafeRelease(pDepthFrame);
	SafeRelease(pMultiSourceFrame);
	return hr;
}

HRESULT KinectHandler::GetColorDepthAndBody(RGBQUAD* &color, BYTE* &bodyIndex, UINT16*& depthBuffer, float*& bodies, int*& bodyTracked, CameraSpacePoint*& headPositions)
{
	if (!m_pMultiFrameReader)
	{
		cout << "No frame reader!" << endl;
		return E_FAIL;
	}

	IColorFrame* pColorFrame = NULL;
	IDepthFrame* pDepthFrame = NULL;
	IBodyIndexFrame*  pBodyIndexFrame = NULL;
	IBodyFrame* pBodyFrame = NULL;
	IMultiSourceFrame* pMultiSourceFrame = NULL;
	HRESULT hr = m_pMultiFrameReader->AcquireLatestFrame(&pMultiSourceFrame);

	if (SUCCEEDED(hr))
	{
		IColorFrameReference* pColorFrameReference = NULL;
		hr = pMultiSourceFrame->get_ColorFrameReference(&pColorFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pColorFrameReference->AcquireFrame(&pColorFrame);
		}

		IDepthFrameReference* pDepthFrameReference = NULL;
		hr = pMultiSourceFrame->get_DepthFrameReference(&pDepthFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pDepthFrameReference->AcquireFrame(&pDepthFrame);
		}

		IBodyIndexFrameReference* pBodyIndexFrameReference = NULL;
		hr = pMultiSourceFrame->get_BodyIndexFrameReference(&pBodyIndexFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pBodyIndexFrameReference->AcquireFrame(&pBodyIndexFrame);
		}

		IBodyFrameReference* pBodyFrameReference = NULL;
		hr = pMultiSourceFrame->get_BodyFrameReference(&pBodyFrameReference);
		if (SUCCEEDED(hr))
		{
			hr = pBodyFrameReference->AcquireFrame(&pBodyFrame);
		}

		SafeRelease(pColorFrameReference);
		SafeRelease(pDepthFrameReference);
		SafeRelease(pBodyIndexFrameReference);
		SafeRelease(pBodyFrameReference);
	}


	if (SUCCEEDED(hr) && pColorFrame != NULL && pDepthFrame != NULL && pBodyIndexFrame != NULL)
	{
		#pragma omp parallel
		{
			#pragma omp sections
			{
				#pragma omp section
				{
					INT64 nTime = 0;
					IFrameDescription* pColorFrameDescription = NULL;
					int nColorWidth = 0;
					int nColorHeight = 0;
					ColorImageFormat imageFormat = ColorImageFormat_None;
					UINT nColorBufferSize = 0;
					RGBQUAD *pColorBuffer = NULL;

					hr = pColorFrame->get_RelativeTime(&nTime);

					if (SUCCEEDED(hr))
					{
						hr = pColorFrame->get_FrameDescription(&pColorFrameDescription);
					}
					if (SUCCEEDED(hr))
					{
						hr = pColorFrameDescription->get_Width(&nColorWidth);
					}
					if (SUCCEEDED(hr))
					{
						hr = pColorFrameDescription->get_Height(&nColorHeight);
					}
					if (SUCCEEDED(hr))
					{
						hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
					}
					if (SUCCEEDED(hr))
					{
						if (imageFormat == ColorImageFormat_Bgra)
						{
							hr = pColorFrame->AccessRawUnderlyingBuffer(&nColorBufferSize, reinterpret_cast<BYTE**>(&pColorBuffer));
						}
						else if (m_pColorRGBX)
						{
							pColorBuffer = m_pColorRGBX;
							nColorBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
							hr = pColorFrame->CopyConvertedFrameDataToArray(nColorBufferSize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Bgra);
						}
						else
						{
							cout << "FAILED" << endl;
							hr = E_FAIL;
						}
					}

					if (SUCCEEDED(hr))
					{
						color = pColorBuffer;
					}
				}
				///===========================================////

				#pragma omp section
				{
					INT64 nTime = 0;
					IFrameDescription* pDepthFrameDescription = NULL;
					int nDepthWidth = 0;
					int nDepthHeight = 0;
					USHORT nDepthMinReliableDistance = 0;
					USHORT nDepthMaxDistance = 0;
					UINT nDepthBufferSize = 0;
					UINT16 *pDepthBuffer = NULL;

					hr = pDepthFrame->get_RelativeTime(&nTime);

					if (SUCCEEDED(hr))
					{
						hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);
					}
					if (SUCCEEDED(hr))
					{
						hr = pDepthFrameDescription->get_Width(&nDepthWidth);
					}
					if (SUCCEEDED(hr))
					{
						hr = pDepthFrameDescription->get_Height(&nDepthHeight);
					}
					if (SUCCEEDED(hr))
					{
						hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
					}
					if (SUCCEEDED(hr))
					{
						// In order to see the full range of depth (including the less reliable far field depth)
						// we are setting nDepthMaxDistance to the extreme potential depth threshold
						nDepthMaxDistance = USHRT_MAX;

						// Note:  If you wish to filter by reliable depth distance, uncomment the following line.
						//// hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
					}

					if (SUCCEEDED(hr))
					{
						hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &pDepthBuffer);
					}

					if (m_pDepthRawBuffer)
					{
						hr = pDepthFrame->CopyFrameDataToArray((cDepthWidth * cDepthHeight), m_pDepthRawBuffer);
						if (SUCCEEDED(hr)) depthBuffer = m_pDepthRawBuffer;
					}

					SafeRelease(pDepthFrameDescription);
				}

				// Body Index data

				#pragma omp section
				{

					IFrameDescription* pBodyIndexFrameDescription = NULL;
					int nBodyIndexWidth = 0;
					int nBodyIndexHeight = 0;
					UINT nBodyIndexBufferSize = 0;
					BYTE *pBodyIndexBuffer = NULL;

					if (SUCCEEDED(hr))
					{
						hr = pBodyIndexFrame->get_FrameDescription(&pBodyIndexFrameDescription);
					}

					if (SUCCEEDED(hr))
					{
						hr = pBodyIndexFrameDescription->get_Width(&nBodyIndexWidth);
					}

					if (SUCCEEDED(hr))
					{
						hr = pBodyIndexFrameDescription->get_Height(&nBodyIndexHeight);
					}

					cout << "width:" << nBodyIndexWidth << " height:" << nBodyIndexHeight << endl;

					if (SUCCEEDED(hr))
					{
						hr = pBodyIndexFrame->AccessUnderlyingBuffer(&nBodyIndexBufferSize, &pBodyIndexBuffer);
					}

					if (pBodyIndexBuffer)
					{
						hr = pBodyIndexFrame->CopyFrameDataToArray((cDepthWidth * cDepthHeight), m_pBodyIndexBuffer);
						if (SUCCEEDED(hr)) bodyIndex = m_pBodyIndexBuffer;
					}

					SafeRelease(pBodyIndexFrameDescription);
				}

				//Body frame // skeleton
				#pragma omp section
				{

					for (int k = 0; k < BODY_COUNT; k++)
					{
						CameraSpacePoint cameraSpacePoint = { 0.0f, 0.7f, 0.0f };
						headPositions[k] = cameraSpacePoint;
					}

					INT64 nTime = 0;

					delete[] m_pJointVertices;
					m_pJointVertices = new float[JointType_Count * BODY_COUNT * 3 * 2];

					hr = pBodyFrame->get_RelativeTime(&nTime);

					IBody* ppBodies[BODY_COUNT] = { 0 };

					if (SUCCEEDED(hr))
					{
						hr = pBodyFrame->GetAndRefreshBodyData(_countof(ppBodies), ppBodies);
					}
					
					if (SUCCEEDED(hr))
					{
						for (int k = 0; k < (int)BODY_COUNT; ++k)
						{
							IBody* pBody = ppBodies[k];
							if (pBody)
							{
								BOOLEAN bTracked = false;
								HRESULT hr = pBody->get_IsTracked(&bTracked);

								if (SUCCEEDED(hr) && bTracked)
								{
									Joint joints[JointType_Count];
									hr = pBody->GetJoints(_countof(joints), joints);
									if (SUCCEEDED(hr))
									{
										bodyTracked[k] = 1;
										for (int jn = 0; jn < _countof(joints); ++jn)
										{
											if (joints[jn].JointType == JointType_Head)
											{
												headPositions[k] = joints[jn].Position;
											}

											m_pJointVertices[((jn * 6) + 0) + JointType_Count*k*6] = joints[jn].Position.X;
											m_pJointVertices[((jn * 6) + 1) + JointType_Count*k*6] = joints[jn].Position.Y;
											m_pJointVertices[((jn * 6) + 2) + JointType_Count*k*6] = joints[jn].Position.Z;

											m_pJointVertices[((jn * 6) + 3) + JointType_Count*k*6] = 255 / 255.0;
											m_pJointVertices[((jn * 6) + 4) + JointType_Count*k*6] = 0 / 255.0;
											m_pJointVertices[((jn * 6) + 5) + JointType_Count*k*6] = 0 / 255.0;

										}
									}
									else
									{
										bodyTracked[k] = 0;
									}
								}
							}
						}

						bodies = m_pJointVertices;
					}

					for (int i = 0; i < _countof(ppBodies); ++i)
					{
						SafeRelease(ppBodies[i]);
					}
				}
			}
		}
	}
	else
	{
		cout << "Acquire last frame FAILED " << endl;
		hr = E_FAIL;
		SafeRelease(pColorFrame);
		SafeRelease(pDepthFrame);
		SafeRelease(pBodyIndexFrame);
		SafeRelease(pBodyFrame);
		SafeRelease(pMultiSourceFrame);
		return hr;
	}

	SafeRelease(pDepthFrame);
	SafeRelease(pColorFrame);
	SafeRelease(pBodyIndexFrame);
	SafeRelease(pBodyFrame);
	SafeRelease(pMultiSourceFrame);
	return hr;
}
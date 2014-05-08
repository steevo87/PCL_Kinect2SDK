
/*
Copyright (C) 2014 Steven Hickson

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

*/
// TestVideoSegmentation.cpp : Defines the entry point for the console application.
//

#include "Microsoft_grabber2.h"

using namespace std;
using namespace cv;

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

DWORD ProcessThread(LPVOID pParam) {
	pcl::Microsoft2Grabber *p = (pcl::Microsoft2Grabber*) pParam;
	p->ProcessThreadInternal();
	return 0;
}

template <typename T> inline T Clamp(T a, T minn, T maxx)
{ return (a < minn) ? minn : ( (a > maxx) ? maxx : a ); }

namespace pcl {
	Microsoft2Grabber::Microsoft2Grabber(const int instance, bool depthOnly) {
		HRESULT hr;
		int num = 0;
		m_person = m_depthStarted = m_videoStarted = m_audioStarted = m_infraredStarted = false;
		hStopEvent = NULL;
		hKinectThread = NULL;
		m_largeCloud = false;

		hr = GetDefaultKinectSensor(&m_pKinectSensor);
		if (FAILED(hr)) {
			throw exception("Error could not get default kinect sensor");
		}

		if (m_pKinectSensor) {
			hr = m_pKinectSensor->get_CoordinateMapper(&m_pCoordinateMapper);
			hr = m_pKinectSensor->Open();
			if (SUCCEEDED(hr)) {
				
				if (depthOnly) {
					hr = m_pKinectSensor->OpenMultiSourceFrameReader(
						FrameSourceTypes::FrameSourceTypes_Depth, // | FrameSourceTypes::FrameSourceTypes_Color | FrameSourceTypes::FrameSourceTypes_BodyIndex,
						&m_pMultiSourceFrameReader);
				} else {
					hr = m_pKinectSensor->OpenMultiSourceFrameReader(
						FrameSourceTypes::FrameSourceTypes_Depth | FrameSourceTypes::FrameSourceTypes_Color | FrameSourceTypes::FrameSourceTypes_BodyIndex,
						&m_pMultiSourceFrameReader);
				}

				if (SUCCEEDED(hr))
				{
					m_videoStarted = m_depthStarted = true;
				} else
					throw exception("Failed to Open Kinect Multisource Stream");
			}
		}

		if (!m_pKinectSensor || FAILED(hr)) {
			throw exception("No ready Kinect found");
		}
		m_colorSize = Size(cColorWidth, cColorHeight);
		m_depthSize = Size(cDepthWidth, cDepthHeight);
		m_pColorRGBX = new RGBQUAD[cColorWidth * cColorHeight];
		m_pColorCoordinates = new ColorSpacePoint[cDepthHeight * cDepthWidth];
		m_pCameraSpacePoints = new CameraSpacePoint[cColorHeight * cColorWidth];

		// create callback signals
		image_signal_             = createSignal<sig_cb_microsoft_image> ();
		depth_image_signal_    = createSignal<sig_cb_microsoft_depth_image> ();
		image_depth_image_signal_    = createSignal<sig_cb_microsoft_image_depth_image> ();
		point_cloud_rgba_signal_  = createSignal<sig_cb_microsoft_point_cloud_rgba> ();
		all_data_signal_  = createSignal<sig_cb_microsoft_all_data> ();
		/*ir_image_signal_       = createSignal<sig_cb_microsoft_ir_image> ();
		point_cloud_signal_    = createSignal<sig_cb_microsoft_point_cloud> ();
		point_cloud_i_signal_  = createSignal<sig_cb_microsoft_point_cloud_i> ();
		point_cloud_rgb_signal_   = createSignal<sig_cb_microsoft_point_cloud_rgb> ();
		*/ 
		if (!depthOnly) {
			rgb_sync_.addCallback (boost::bind (&Microsoft2Grabber::imageDepthImageCallback, this, _1, _2));
		}
	}

	void Microsoft2Grabber::start() {
		block_signals();
		//GetCameraSettings();
		/*hDepthMutex = CreateMutex(NULL,false,NULL);
		if(hDepthMutex == NULL)
		throw exception("Could not create depth mutex");
		hColorMutex = CreateMutex(NULL,false,NULL);
		if(hColorMutex == NULL)
		throw exception("Could not create color mutex");*/
		//hFrameEvent = (WAITABLE_HANDLE)CreateEvent(NULL,FALSE,FALSE,NULL);
		HRESULT hr = m_pMultiSourceFrameReader->SubscribeMultiSourceFrameArrived(&hFrameEvent);
		if(FAILED(hr)) {
			throw exception("Couldn't subscribe frame");
		}
		hStopEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
		hKinectThread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE)&ProcessThread, this, 0, NULL );
		//boost::this_thread::sleep (boost::posix_time::seconds (1));
		unblock_signals();
	}

	void Microsoft2Grabber::stop() {
		//stop the ProcessThread
		if(hStopEvent != NULL) {
			//signal the process to stop
			SetEvent(hStopEvent);
			if(hKinectThread != NULL) {
				WaitForSingleObject(hKinectThread,INFINITE);
				CloseHandle(hKinectThread);
				hKinectThread = NULL;
			}
			CloseHandle(hStopEvent);
			hStopEvent = NULL;
			m_pMultiSourceFrameReader->UnsubscribeMultiSourceFrameArrived(hFrameEvent);
			CloseHandle((HANDLE)hFrameEvent);
			hFrameEvent = NULL;
			/*CloseHandle(hDepthMutex);
			hDepthMutex = NULL;
			CloseHandle(hColorMutex);
			hColorMutex = NULL;*/
		}
		if (m_pColorRGBX) {
			delete [] m_pColorRGBX;
			m_pColorRGBX = NULL;
		}
		if(m_pColorCoordinates) {
			delete [] m_pColorCoordinates;
			m_pColorCoordinates = NULL;
		}
		if(m_pCameraSpacePoints) {
			delete [] m_pCameraSpacePoints;
			m_pCameraSpacePoints = NULL;
		}
	}

	bool Microsoft2Grabber::isRunning () const {
		return (!(hKinectThread == NULL));
	}

	Microsoft2Grabber::~Microsoft2Grabber() {
		Release();
	}

	bool Microsoft2Grabber::GetCameraSettings() {
		/*CameraSettings = NULL;
		if(S_OK == kinectInstance->NuiGetColorCameraSettings(&CameraSettings))
		CameraSettingsSupported = true;
		else
		CameraSettingsSupported = false;*/
		return CameraSettingsSupported;
	}

	void Microsoft2Grabber::ProcessThreadInternal() {
		HANDLE handles[] = { reinterpret_cast<HANDLE>(hFrameEvent) };
		int idx;
		bool quit = false;
		while(!quit) {
			// Wait for any of the events to be signalled
			idx = WaitForMultipleObjects(1,handles,FALSE,100);
			switch(idx) {
			case WAIT_TIMEOUT:
				continue;
			case WAIT_OBJECT_0:
				IMultiSourceFrameArrivedEventArgs *pFrameArgs = nullptr;
				HRESULT hr = m_pMultiSourceFrameReader->GetMultiSourceFrameArrivedEventData(hFrameEvent,&pFrameArgs);
				//frame arrived
				FrameArrived(pFrameArgs);
				pFrameArgs->Release();
				break;
				/*case WAIT_OBJECT_0 + 1:
				quit = true;
				continue;*/
			}
			//if(WaitForSingleObject(hStopEvent,1) == WAIT_OBJECT_0)
			//	quit = true;
			//else {
			//	//Get the newest frame info
			//	GetNextFrame();
			//}
		}
	}

	void Microsoft2Grabber::Release() {
		try {
			//clean up stuff here
			stop();
			if(m_pKinectSensor) {
				//Shutdown NUI and Close handles
				if (m_pMultiSourceFrameReader)
					SafeRelease(m_pMultiSourceFrameReader);
				if(m_pCoordinateMapper)
					SafeRelease(m_pCoordinateMapper);
				// close the Kinect Sensor
				if (m_pKinectSensor)
					m_pKinectSensor->Close();

				SafeRelease(m_pKinectSensor);
			}
		} catch(...) {
			//destructor never throws
		}
	}

	string Microsoft2Grabber::getName () const {
		return std::string ("Microsoft2Grabber");
	}

	float Microsoft2Grabber::getFramesPerSecond () const {
		return 30.0f;
	}

	void Microsoft2Grabber::BodyIndexFrameArrived(IBodyIndexFrameReference* pBodyIndexFrameReference) {
		IBodyIndexFrame* pBodyIndexFrame = NULL;
		HRESULT hr = pBodyIndexFrameReference->AcquireFrame(&pBodyIndexFrame);
		if(FAILED(hr))
			return;
		//cout << "got a body index frame" << endl;
		IFrameDescription* pBodyIndexFrameDescription = NULL;
		int nBodyIndexWidth = 0;
		int nBodyIndexHeight = 0;
		UINT nBodyIndexBufferSize = 0;
		BYTE *pBodyIndexBuffer = NULL;

		// get body index frame data
		if (SUCCEEDED(hr)) {
			hr = pBodyIndexFrame->get_FrameDescription(&pBodyIndexFrameDescription);
		}
		if (SUCCEEDED(hr)) {
			hr = pBodyIndexFrameDescription->get_Width(&nBodyIndexWidth);
		}
		if (SUCCEEDED(hr)) {
			hr = pBodyIndexFrameDescription->get_Height(&nBodyIndexHeight);
		}
		if (SUCCEEDED(hr)) {
			hr = pBodyIndexFrame->AccessUnderlyingBuffer(&nBodyIndexBufferSize, &pBodyIndexBuffer);            
		}
		SafeRelease(pBodyIndexFrameDescription);
		SafeRelease(pBodyIndexFrame);
	}

	void Microsoft2Grabber::FrameArrived(IMultiSourceFrameArrivedEventArgs *pArgs, bool depthOnly) {
		HRESULT hr;
		IMultiSourceFrameReference *pFrameReference = nullptr;

		//cout << "got a valid frame" << endl;
		hr = pArgs->get_FrameReference(&pFrameReference);
		if (SUCCEEDED(hr))
		{
			IMultiSourceFrame *pFrame = nullptr;
			hr = pFrameReference->AcquireFrame(&pFrame);
			if (FAILED(hr)) {
				cout << "fail on AcquireFrame" << endl;
			}
			IColorFrameReference* pColorFrameReference = nullptr;
			IDepthFrameReference* pDepthFrameReference = nullptr;
			IBodyIndexFrameReference* pBodyIndexFrameReference = nullptr;
			hr = pFrame->get_DepthFrameReference(&pDepthFrameReference);
			if (SUCCEEDED(hr))
				DepthFrameArrived(pDepthFrameReference);
			SafeRelease(pDepthFrameReference);

			if (!depthOnly) {
				hr = pFrame->get_ColorFrameReference(&pColorFrameReference);
				if (SUCCEEDED(hr))
					ColorFrameArrived(pColorFrameReference);
				SafeRelease(pColorFrameReference);

				hr = pFrame->get_BodyIndexFrameReference(&pBodyIndexFrameReference);
				if (SUCCEEDED(hr))
					BodyIndexFrameArrived(pBodyIndexFrameReference);
				SafeRelease(pBodyIndexFrameReference);
			}

			pFrameReference->Release();
		}
	}

#pragma endregion

	//Camera Functions
#pragma region Camera
	void Microsoft2Grabber::ColorFrameArrived(IColorFrameReference* pColorFrameReference) {
		IColorFrame* pColorFrame = NULL;
		HRESULT hr = pColorFrameReference->AcquireFrame(&pColorFrame);
		if(FAILED(hr)) {
			//cout << "Couldn't acquire color frame" << endl;
			return;
		}
		//cout << "got a color frame" << endl;
		INT64 nColorTime = 0;
		IFrameDescription* pColorFrameDescription = NULL;
		int nColorWidth = 0;
		int nColorHeight = 0;
		ColorImageFormat imageFormat = ColorImageFormat_None;
		UINT nColorBufferSize = 0;
		RGBQUAD *pColorBuffer = NULL;

		// get color frame data
		hr = pColorFrame->get_RelativeTime(&nColorTime);
		if (SUCCEEDED(hr)) {
			hr = pColorFrame->get_FrameDescription(&pColorFrameDescription);
		}
		if (SUCCEEDED(hr)) {
			hr = pColorFrameDescription->get_Width(&nColorWidth);
		}
		if (SUCCEEDED(hr)) {
			hr = pColorFrameDescription->get_Height(&nColorHeight);
		}
		if (SUCCEEDED(hr)) {
			hr = pColorFrame->get_RawColorImageFormat(&imageFormat);
		}
		if (SUCCEEDED(hr)) {
			if (imageFormat == ColorImageFormat_Bgra) {
				hr = pColorFrame->AccessRawUnderlyingBuffer(&nColorBufferSize, reinterpret_cast<BYTE**>(&pColorBuffer));
			} else if (m_pColorRGBX) {
				pColorBuffer = m_pColorRGBX;
				nColorBufferSize = cColorWidth * cColorHeight * sizeof(RGBQUAD);
				hr = pColorFrame->CopyConvertedFrameDataToArray(nColorBufferSize, reinterpret_cast<BYTE*>(pColorBuffer), ColorImageFormat_Bgra);
			} else {
				hr = E_FAIL;
			}
			if(SUCCEEDED(hr)) {
				//WaitForSingleObject(hColorMutex,INFINITE);
				//cout << "creating the image" << endl;
				Mat tmp = Mat(m_colorSize, COLOR_PIXEL_TYPE, pColorBuffer, Mat::AUTO_STEP);
				boost::shared_ptr<Mat> img(new Mat());
				*img = tmp.clone();
				m_rgbTime = nColorTime;
				if (image_signal_->num_slots () > 0) {
					//cout << "img signal num slot!" << endl;
					image_signal_->operator()(img);
				}

				if (num_slots<sig_cb_microsoft_point_cloud_rgba>() > 0 || all_data_signal_->num_slots() > 0 || image_depth_image_signal_->num_slots() > 0) {
					rgb_sync_.add0 (img, m_rgbTime);
				}
				
				//ReleaseMutex(hColorMutex);
			}
		}
		SafeRelease(pColorFrameDescription);
		SafeRelease(pColorFrame);
	}
#pragma endregion

	//Depth Functions
#pragma region Depth
	void Microsoft2Grabber::DepthFrameArrived(IDepthFrameReference* pDepthFrameReference) {
		IDepthFrame* pDepthFrame = NULL;
		HRESULT hr = pDepthFrameReference->AcquireFrame(&pDepthFrame);
		//HRESULT hr = pDepthFrameReference->AcquireLatestFrame(&pDepthFrame);
		if(FAILED(hr))
			return;
		//cout << "got a depth frame" << endl;
		INT64 nDepthTime = 0;
		IFrameDescription* pDepthFrameDescription = NULL;
		int nDepthWidth = 0;
		int nDepthHeight = 0;
		UINT nDepthBufferSize = 0;

		// get depth frame data
		hr = pDepthFrame->get_RelativeTime(&nDepthTime);
		if (SUCCEEDED(hr)) {
			hr = pDepthFrame->get_FrameDescription(&pDepthFrameDescription);
		}
		if (SUCCEEDED(hr)) {
			hr = pDepthFrameDescription->get_Width(&nDepthWidth);
		}
		if (SUCCEEDED(hr)) {
			hr = pDepthFrameDescription->get_Height(&nDepthHeight);
		}
		if (SUCCEEDED(hr)) {
			hr = pDepthFrame->AccessUnderlyingBuffer(&nDepthBufferSize, &m_pDepthBuffer);
			//WaitForSingleObject(hDepthMutex,INFINITE);
			Mat tmp = Mat(m_depthSize, DEPTH_PIXEL_TYPE, m_pDepthBuffer, Mat::AUTO_STEP);
			MatDepth depth_img = *((MatDepth*)&(tmp.clone()));
			m_depthTime = nDepthTime;
			if (depth_image_signal_->num_slots () > 0) {
				depth_image_signal_->operator()(depth_img);
			}
			if (num_slots<sig_cb_microsoft_point_cloud_rgba>() > 0 || all_data_signal_->num_slots() > 0 || image_depth_image_signal_->num_slots() > 0) {
				//rgb_sync_.add1 (depth_img, m_depthTime);
				imageDepthOnlyImageCallback(depth_img);
			}
			
			//ReleaseMutex(hDepthMutex);
		}
		SafeRelease(pDepthFrameDescription);
		SafeRelease(pDepthFrame);
	}
#pragma endregion

#pragma region Cloud
	void Microsoft2Grabber::imageDepthImageCallback (const boost::shared_ptr<Mat> &image,
		const MatDepth &depth_image)
	{
		boost::shared_ptr<PointCloud<PointXYZRGBA>> cloud;
		// check if we have color point cloud slots
		if(point_cloud_rgba_signal_->num_slots() > 0 || all_data_signal_->num_slots() > 0)
			cloud = convertToXYZRGBAPointCloud(image, depth_image);
		if (point_cloud_rgba_signal_->num_slots () > 0)
			point_cloud_rgba_signal_->operator()(cloud);
		if(all_data_signal_->num_slots() > 0) {
			//boost::shared_ptr<KinectData> data (new KinectData(image,depth_image,*cloud));
			//all_data_signal_->operator()(data);
		}

		if(image_depth_image_signal_->num_slots() > 0) {
			float constant = 1.0f;
			image_depth_image_signal_->operator()(image,depth_image,constant);
		}
	}

	void Microsoft2Grabber::imageDepthOnlyImageCallback (const MatDepth &depth_image)
	{
		boost::shared_ptr<PointCloud<PointXYZRGBA>> cloud;
		// check if we have color point cloud slots
		if(point_cloud_rgba_signal_->num_slots() > 0 || all_data_signal_->num_slots() > 0)
			cloud = convertToXYZRGBAPointCloud(depth_image);
		if (point_cloud_rgba_signal_->num_slots () > 0)
			point_cloud_rgba_signal_->operator()(cloud);
		if(all_data_signal_->num_slots() > 0) {
			//boost::shared_ptr<KinectData> data (new KinectData(image,depth_image,*cloud));
			//all_data_signal_->operator()(data);
		}

		/*
		if(image_depth_image_signal_->num_slots() > 0) {
			float constant = 1.0f;
			image_depth_image_signal_->operator()(image,depth_image,constant);
		}
		*/
	}

	void Microsoft2Grabber::GetPointCloudFromData(const boost::shared_ptr<Mat> &img, const MatDepth &depth, boost::shared_ptr<PointCloud<PointXYZRGBA>> &cloud, bool alignToColor, bool preregistered) const
	{
		if(!img || img->empty() || depth.empty()) {
			cout << "empty img or depth" << endl;
			return;
		}

		UINT16 *pDepth = (UINT16*)depth.data;
		int length = cDepthHeight * cDepthWidth, length2;
		HRESULT hr;
		if(alignToColor) {
			length2 = cColorHeight * cColorWidth;
			hr = m_pCoordinateMapper->MapColorFrameToCameraSpace(length,pDepth,length2,m_pCameraSpacePoints);
			if(FAILED(hr))
				throw exception("Couldn't map to camera!");
		} else {
			hr = m_pCoordinateMapper->MapDepthFrameToCameraSpace(length,pDepth,length,m_pCameraSpacePoints);
			if(FAILED(hr))
				throw exception("Couldn't map to camera!");
			hr = m_pCoordinateMapper->MapCameraPointsToColorSpace(length,m_pCameraSpacePoints,length,m_pColorCoordinates);
			if(FAILED(hr))
				throw exception("Couldn't map color!");
		}

		PointCloud<PointXYZRGBA>::iterator pCloud = cloud->begin();
		ColorSpacePoint *pColor = m_pColorCoordinates;
		CameraSpacePoint *pCamera = m_pCameraSpacePoints;
		float bad_point = std::numeric_limits<float>::quiet_NaN ();
		int x,y, safeWidth = cColorWidth - 1, safeHeight = cColorHeight - 1;
		int width = alignToColor ? cColorWidth : cDepthWidth;
		int height = alignToColor ? cColorHeight : cDepthHeight;
		for(int j = 0; j < height; j++) {
			for(int i = 0; i < width; i++) {
				PointXYZRGBA loc;
				Vec4b color;
				if(!preregistered && !alignToColor) {
					x = Clamp<int>(int(pColor->X),0,safeWidth);
					y = Clamp<int>(int(pColor->Y),0,safeHeight);
					//int index = y * cColorHeight + x;
					color = img->at<Vec4b>(y,x);
				} else
					color = img->at<Vec4b>(j,i);
				loc.b = color[0];
				loc.g = color[1];
				loc.r = color[2];
				loc.a = 255;
				if(pCamera->Z == 0) {
					loc.x = loc.y = loc.z = bad_point;
				} else {
					loc.x = pCamera->X;
					loc.y = pCamera->Y;
					loc.z = pCamera->Z;
				}
				//cout << "Iter: " << i << ", " << j << endl;
				*pCloud = loc;
				++pCamera; ++pCloud; ++pColor;
			}
		}
		img->release();
	}

	void Microsoft2Grabber::GetPointCloudFromColorlessData(const MatDepth &depth, boost::shared_ptr<PointCloud<PointXYZRGBA>> &cloud, bool alignToColor, bool preregistered) const
	{
		if(depth.empty()) {
			cout << "empty depth" << endl;
			return;
		}

		UINT16 *pDepth = (UINT16*)depth.data;
		int length = cDepthHeight * cDepthWidth, length2;
		HRESULT hr;
		if(alignToColor) {
			length2 = cColorHeight * cColorWidth;
			hr = m_pCoordinateMapper->MapColorFrameToCameraSpace(length,pDepth,length2,m_pCameraSpacePoints);
			if(FAILED(hr))
				throw exception("Couldn't map to camera!");
		} else {
			hr = m_pCoordinateMapper->MapDepthFrameToCameraSpace(length,pDepth,length,m_pCameraSpacePoints);
			if(FAILED(hr))
				throw exception("Couldn't map to camera!");
			hr = m_pCoordinateMapper->MapCameraPointsToColorSpace(length,m_pCameraSpacePoints,length,m_pColorCoordinates);
			if(FAILED(hr))
				throw exception("Couldn't map color!");
		}

		PointCloud<PointXYZRGBA>::iterator pCloud = cloud->begin();
		ColorSpacePoint *pColor = m_pColorCoordinates;
		CameraSpacePoint *pCamera = m_pCameraSpacePoints;
		float bad_point = std::numeric_limits<float>::quiet_NaN ();
		int x,y, safeWidth = cColorWidth - 1, safeHeight = cColorHeight - 1;
		int width = alignToColor ? cColorWidth : cDepthWidth;
		int height = alignToColor ? cColorHeight : cDepthHeight;
		for(int j = 0; j < height; j++) {
			for(int i = 0; i < width; i++) {
				PointXYZRGBA loc;
				Vec4b color;
				if(!preregistered && !alignToColor) {
					x = Clamp<int>(int(pColor->X),0,safeWidth);
					y = Clamp<int>(int(pColor->Y),0,safeHeight);
					//int index = y * cColorHeight + x;
					color = Vec4b(255,255,255,255); // img->at<Vec4b>(y,x);
				} else
					color = Vec4b(255,255,255,255); // img->at<Vec4b>(j,i);
				loc.b = color[0];
				loc.g = color[1];
				loc.r = color[2];
				loc.a = 255;
				if(pCamera->Z == 0) {
					loc.x = loc.y = loc.z = bad_point;
				} else {
					loc.x = pCamera->X;
					loc.y = pCamera->Y;
					loc.z = pCamera->Z;
				}
				//cout << "Iter: " << i << ", " << j << endl;
				*pCloud = loc;
				++pCamera; ++pCloud; ++pColor;
			}
		}
		//img->release();
	}

	boost::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBA>> Microsoft2Grabber::convertToXYZRGBAPointCloud (const boost::shared_ptr<cv::Mat> &image,
		const MatDepth &depth_image) const {
			boost::shared_ptr<PointCloud<PointXYZRGBA> > cloud (new PointCloud<PointXYZRGBA>);

			cloud->header.frame_id =  "/microsoft_rgb_optical_frame";
			cloud->height = m_largeCloud ? cColorHeight : cDepthHeight;
			cloud->width = m_largeCloud ? cColorWidth : cDepthWidth;
			cloud->is_dense = false;
			cloud->points.resize (cloud->height * cloud->width);
			GetPointCloudFromData(image,depth_image,cloud,m_largeCloud,false);
			cloud->sensor_origin_.setZero ();
			cloud->sensor_orientation_.w () = 1.0;
			cloud->sensor_orientation_.x () = 0.0;
			cloud->sensor_orientation_.y () = 0.0;
			cloud->sensor_orientation_.z () = 0.0;
			return (cloud);
	}

	boost::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBA>> Microsoft2Grabber::convertToXYZRGBAPointCloud (const MatDepth &depth_image) const {
			boost::shared_ptr<PointCloud<PointXYZRGBA> > cloud (new PointCloud<PointXYZRGBA>);

			cloud->header.frame_id =  "/microsoft_rgb_optical_frame";
			cloud->height = m_largeCloud ? cColorHeight : cDepthHeight;
			cloud->width = m_largeCloud ? cColorWidth : cDepthWidth;
			cloud->is_dense = false;
			cloud->points.resize (cloud->height * cloud->width);
			GetPointCloudFromColorlessData(depth_image,cloud,m_largeCloud,false);
			cloud->sensor_origin_.setZero ();
			cloud->sensor_orientation_.w () = 1.0;
			cloud->sensor_orientation_.x () = 0.0;
			cloud->sensor_orientation_.y () = 0.0;
			cloud->sensor_orientation_.z () = 0.0;
			return (cloud);
	}

#pragma endregion
};
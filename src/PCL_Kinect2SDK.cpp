#include <iostream>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>
#include <pcl/point_types.h>
#include <pcl/point_cloud.h>
#include <pcl/common/time.h>
#include <pcl/features/integral_image_normal.h>
#include <pcl/visualization/cloud_viewer.h>

#include "Microsoft_grabber2.h"
#include <pcl/visualization/cloud_viewer.h>
/*#include <FaceTrackLib.h>
#include <KinectInteraction.h>
#include <NuiKinectFusionApi.h>
#include <NuiKinectFusionDepthProcessor.h>
#include <NuiKinectFusionVolume.h>*/

#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <deque>

using namespace std;
using namespace pcl;
using namespace cv;

class SimpleMicrosoftViewer
{
public:
	SimpleMicrosoftViewer () : viewer(new pcl::visualization::PCLVisualizer("cloud viewer")), normals(new pcl::PointCloud<pcl::Normal>), sharedCloud(new pcl::PointCloud<pcl::PointXYZRGBA>), first(false), update(false) {}

	void GetMatFromCloud(const PointCloud<PointXYZRGBA> &cloud, Mat &img) {
		img = Mat(cloud.height,cloud.width,CV_8UC3);
		Mat_<Vec3b>::iterator pI = img.begin<Vec3b>();
		PointCloud<PointXYZRGBA>::const_iterator pC = cloud.begin();
		while(pC != cloud.end()) {
			(*pI)[0] = pC->b;
			(*pI)[1] = pC->g;
			(*pI)[2] = pC->r;
			++pI; ++pC;
		}
	}

	void GetDepthFromCloud(const PointCloud<PointXYZRGBA> &cloud, Mat &depth) {
		depth = Mat::zeros(cloud.height,cloud.width,CV_16UC1);
		Mat_<unsigned short>::iterator pI = depth.begin<unsigned short>();
		PointCloud<PointXYZRGBA>::const_iterator pC = cloud.begin();
		while(pC != cloud.end()) {
			*pI = 1000*pC->z;
			++pI; ++pC;
		}
	}

	void cloud_cb_ (const boost::shared_ptr<const pcl::PointCloud<pcl::PointXYZRGBA> >&data)
	{
		// estimate normals
		/*imshow("image", *(data->image));
		imshow("depth", data->depth);
		waitKey(2);*/
		if(!data->empty()) {
			normalMutex.lock();
			copyPointCloud(*data,*sharedCloud);
			/*Mat tmp;
			GetDepthFromCloud(*sharedCloud,tmp);
			imshow("image",tmp);*/
			pcl::IntegralImageNormalEstimation<pcl::PointXYZRGBA, pcl::Normal> ne;
			ne.setNormalEstimationMethod (ne.AVERAGE_3D_GRADIENT);
			ne.setMaxDepthChangeFactor(0.02f);
			ne.setNormalSmoothingSize(10.0f);
			ne.setInputCloud(sharedCloud);
			ne.compute(*normals);
			update = true;
			normalMutex.unlock();
			waitKey(1);
		}
	}

	void cloud_save_cb_ (const boost::shared_ptr<const pcl::PointCloud<pcl::PointXYZRGBA> >&data)
	{
		if(!data->empty())
			clouds.push_back(data);
	}

	void image_cb_ (const boost::shared_ptr<Mat> &image) {
		if(image && !image->empty()) {
			imshow("image",*image);
			waitKey(1);
			//image->release();
		}
		/*cout << "writing image" << endl;
		images.push_back(*image);*/
	}

	void depth_cb_ (const MatDepth &depth) {
		imshow("depth",depth);
		waitKey(1);
		//((Mat)(depth)).release();
		/*cout << "writing depth" << endl;
		depths.push_back(depth);*/
	}

	void savedata() {
		// create a new grabber for OpenNI devices
		pcl::Grabber* my_interface = new pcl::Microsoft2Grabber();

		boost::function<void (const boost::shared_ptr<const pcl::PointCloud<pcl::PointXYZRGBA> >&)> f =
			boost::bind (&SimpleMicrosoftViewer::cloud_save_cb_, this, _1);
		my_interface->registerCallback (f);
		my_interface->start ();
		boost::this_thread::sleep(boost::posix_time::milliseconds(30));
		int num = 0;
		while(1) {
			//cout << "buffer empty" << endl;
			if(!clouds.empty()) {
				char buffer[100];
				sprintf(buffer,"%05d.pcd",num);
				pcl::io::savePCDFileBinary<PointXYZRGBA>(buffer,*clouds.front());
				clouds.pop_front();
				num++;
				cout << "Wrote images" <<endl;
			}
		}
	}

	void run ()
	{
		bool depthOnlyMode = true;

		// create a new grabber for OpenNI devices
		pcl::Microsoft2Grabber* my_interface = new pcl::Microsoft2Grabber(0, depthOnlyMode);

		// make callback function from member function
		boost::function<void (const boost::shared_ptr<const pcl::PointCloud<pcl::PointXYZRGBA> >&)> f =
			boost::bind (&SimpleMicrosoftViewer::cloud_cb_, this, _1);
		//boost::function<void (const boost::shared_ptr<Mat>&)> f2 =
		//	boost::bind (&SimpleMicrosoftViewer::image_cb_, this, _1);
		//boost::function<void (const MatDepth&)> f3 =
		//	boost::bind (&SimpleMicrosoftViewer::depth_cb_, this, _1);

		my_interface->registerCallback (f);
		//my_interface->registerCallback (f2);
		//my_interface->registerCallback (f3);

		//viewer.setBackgroundColor(0.0, 0.0, 0.5);
		//my_interface->SetLargeCloud();
		my_interface->start ();
		Sleep(30);
		//while(1)
		//	boost::this_thread::sleep (boost::posix_time::seconds (1));
		while (!viewer->wasStopped())
		{
			normalMutex.lock();
			if(update) {
				viewer->removePointCloud("cloud");
				viewer->removePointCloud("original");
				viewer->addPointCloud(sharedCloud,"original");
				viewer->addPointCloudNormals<pcl::PointXYZRGBA,pcl::Normal>(sharedCloud, normals);
				update = false;
			}
			viewer->spinOnce();
			normalMutex.unlock();
		}

		my_interface->stop ();
	}

	void run2() {
		// estimate normals
		PointCloud<PointXYZRGBA>::Ptr cloud(new PointCloud<PointXYZRGBA>());
		pcl::io::loadPCDFile("C:/Users/Steve/Documents/test.pcd",*cloud);
		pcl::PointCloud<pcl::Normal>::Ptr normals (new pcl::PointCloud<pcl::Normal>);

		pcl::IntegralImageNormalEstimation<pcl::PointXYZRGBA, pcl::Normal> ne;
		ne.setNormalEstimationMethod (ne.AVERAGE_3D_GRADIENT);
		ne.setMaxDepthChangeFactor(0.02f);
		ne.setNormalSmoothingSize(10.0f);
		ne.setInputCloud(cloud);
		ne.compute(*normals);

		// visualize normals
		//viewer.addPointCloud<PointXYZRGBA>(cloud,"original");
		/*viewer->addPointCloudNormals<pcl::PointXYZRGBA,pcl::Normal>(cloud, normals);
		while (!viewer->wasStopped())
		viewer->spinOnce(100);*/
	}

	deque<const boost::shared_ptr<const pcl::PointCloud<pcl::PointXYZRGBA> > > clouds;
	boost::shared_ptr<pcl::PointCloud<pcl::Normal>> normals;
	boost::shared_ptr<pcl::PointCloud<pcl::PointXYZRGBA>> sharedCloud;
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer;
	bool first, update;
	boost::mutex normalMutex;
};

int
	main (int argc, char** argv)
{
	PointCloud<PointXYZ> depth;
	PointCloud<PointXYZRGB> color;
	PointCloud<PointXYZRGB> cloud;
	try {
		SimpleMicrosoftViewer v;
		v.run();
	} catch (pcl::PCLException e) {
		cout << e.detailedMessage() << endl;
	} catch (std::exception &e) {
		cout << e.what() << endl;
	}
	cin.get();
	return (0);
}
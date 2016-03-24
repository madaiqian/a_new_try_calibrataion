#include <iostream>
#include <cstdio>
#include <cstdlib>
#include "opencv2/opencv.hpp"
#include <cstring>
#include <algorithm>
#include  <map>
#include <string>

using namespace cv;
using namespace std;

#define ChessBoardWidth 7
#define ChessBoardHeight 7
#define WIDTH 720
#define HEIGHT 480
#define ImageNum 3
#define beishu 1  
#define range 40

string str_head = "��������\\4\\";
string str[10] = { "11", "22", "00", "1", "2", "8" };

int img_height, img_width;
int checked_img_num = 0;

double sq_sz = 41.27;        //��ʵ���̸񷽿���
double f = 161.85;           //����
double pixel_size = 0.006;   //LUTϵ��
double ellip = 1.125;        //��Բϵ��
vector<double> table;        //LUT��
double step;                 //LUT����
int LUT_num;                 //LUT����

Size s = Size(WIDTH,HEIGHT);//Size(1440,960);

vector<Point2f> corners;               //���̸�ǵ㼯
vector<vector<Point3d>>  obj_points;   //��ʵ����㼯(Z=0)
vector<vector<Point2d> > img_points;   //����ƽ��㼯
vector<Point3d> obj_temp;              //���ڴ��һ��ͼ�ĵ㼯



/**********************************************
*  �����̸�
*  ���ҵ�,�򽫵�������ƽ��㼯img_points
**********************************************/
void ready_go()
{
	cout << "Image_list: ";
	for (int i = 0; i < ImageNum; i++)
	{
		Mat img1 = imread(str_head + str[i] + ".jpg");
		cout << str[i] + ".jpg" << " ";
		img_width = img1.cols;
		img_height = img1.rows;

		bool found = findChessboardCorners(img1, Size(ChessBoardWidth,ChessBoardHeight), corners);

		Mat gray;
		cvtColor(img1, gray, CV_BGR2GRAY);
		//cornerSubPix(gray, corners, Size(10, 10), Size(-1, -1), TermCriteria(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 20, 0.03));
		drawChessboardCorners(img1, Size(ChessBoardWidth, ChessBoardHeight), corners, found);

		///*
		namedWindow(str[i], 0);
		imshow(str[i], img1);
		waitKey(10);
		//*/

		//������̸�Ѱ�ҳɹ�����������ƽ��㼯
		if (found)
		{
			vector<Point2d> img_temp;
			for (int j = 0; j < ChessBoardHeight*ChessBoardWidth; j++)
			{
				Point2d temp = corners[j];
				img_temp.push_back(temp);
			}
			img_points.push_back(img_temp);
			checked_img_num++;
		}
	}

	//����������ά����㼯
	for (int i = 0; i < ChessBoardHeight; i++) {
		for (int j = 0; j < ChessBoardWidth; j++) {
			obj_temp.push_back(Point3d(double(j * sq_sz), double(i * sq_sz), 0));
		}
	}
	for (int i = 0; i < checked_img_num; i++) obj_points.push_back(obj_temp);
}


/**********************************************
*  xml���K,D
**********************************************/
void xml_out(Matx33d K,Vec4d D)
{
	//����XML�ļ�д��  
	FileStorage fs("intrinsics.xml", FileStorage::WRITE);
	Mat mat = Mat(K);
	fs << "intrinsics" << mat;
	mat = Mat(D);
	fs << "coefficient" << mat;
	fs.release();
}


/**********************************************
*LUT����
**********************************************/
void init_LUT()
{
	int LUT_num = 0;
	FILE *in;
	in = fopen("curve101_LUT.txt", "r");
	fscanf(in, "%lf", &step);
	double angle;
	while (fscanf(in, "%lf", &angle) != NULL&&LUT_num<18000)
	{
		table.push_back(angle);
		LUT_num++;
	}
	fclose(in);
}

/**********************************************
*���ݽ���ͼ������(i,j)�����Ӧ������ͼ������(resultx,resulty)
**********************************************/
void correspondence(double i, double j, int xcenter, int ycenter, double* resultx, double* resulty)
{
	//��������ı���
	double theta;
	double ru;
	double x, y;
	double rd;

	//christian
	//i = i * ellip;
	//ycenter = ycenter * ellip;
	ru = sqrt(pow(i - ycenter * beishu, 2) + pow((j - xcenter * beishu)/ellip, 2));
	if (ru != 0)
	{
		//�Զ������,���ʵ�������е������
		theta = atan(ru / f);
		//������۾�ͷ�ڵĽǶ�
		//----LUT----
		if (1)
		{
			double angle = theta * 180 / CV_PI;
			int r = angle / step;
			rd = (table[r] + (table[r + 1] - table[r])*((angle - r*step) / step)) / pixel_size;
		}
		else
		{
			//��ʽ
			rd = 2 * f * sin(theta / 2);
		}
		//������۾�ͷ����Թ��������
		//christian
		x = rd * abs(j - xcenter * beishu) / ru;
		y = rd * abs(i - ycenter * beishu) / ru;
	}
	else
	{
		x = y = 0;
	}

	//christian
	if (i <= ycenter * beishu && j <= xcenter * beishu) { x = -x; y = -y; }
	else if (i <= ycenter * beishu && j > xcenter * beishu) { y = -y; }
	else if (i > ycenter * beishu && j <= xcenter * beishu) { x = -x; }

	//�任Ϊ��������
	*resultx = x*ellip + xcenter;
	*resulty = (y + ycenter);
	return;
}



int main(){


	ready_go();

	cv::Matx33d K,K2;
	cv::Vec4d D;
	int flag = 0;
	flag |= cv::fisheye::CALIB_RECOMPUTE_EXTRINSIC;
	flag |= cv::fisheye::CALIB_CHECK_COND;
	flag |= cv::fisheye::CALIB_FIX_SKEW;
	//cout << "flag: "<<flag << endl;
	//�궨����ڲξ���K�ͻ���ϵ������D
	double calibrate_error=fisheye::calibrate(obj_points, img_points, Size(img_width, img_height), K, D, noArray(), noArray(), flag, TermCriteria(3, 20, 1e-6));
	getOptimalNewCameraMatrix(K, D, s, 1.0, s);
	//�õ������������
	fisheye::estimateNewCameraMatrixForUndistortRectify(K, D, Size(720, 480), cv::noArray(), K2, 0.8, s ,1.0);
	
	
	cout << endl << "K:" << K << endl << "D:" << D << endl << "calibrate_error:  " << calibrate_error << endl;

	xml_out(K, D);

	//���۽�����ͼ��
	for (int i = 0; i <ImageNum; i++)
	{
		Mat output;// = Mat(Size(img_height, img_width), CV_8UC3);
		Mat img1 = imread(str_head + str[i] + ".jpg");
		fisheye::undistortImage(img1, output, K, D, K2, s);
		//namedWindow("img"+str[i], 0);
		//imshow("img"+str[i], output);
		//waitKey();
	}

	//=============================================================================================
	double ChessScore[range][range],ChessScore2[range][range];
	Mat u_img[range][range]; 
	memset(ChessScore, 0, sizeof(ChessScore));
	memset(ChessScore2, 0, sizeof(ChessScore2));
	Mat img = imread(str_head + str[2] + ".jpg");
	init_LUT();
  	Mat P = Mat(K);
	double* T = (double*)(P.data);
	double center_x = *(T + 2), center_y = *(T + 5);
	for (int i = 0; i < range; i++)
		for (int j = 0; j < range; j++)
		{
		     double center_xx = center_x + i - range / 2;
		     double center_yy = center_y + j - range / 2;
			 int u_width = beishu *WIDTH, u_height = beishu*HEIGHT;
			 u_img[i][j] = Mat(Size(u_width, u_height), CV_8UC3);
			 for (int k1 = 0; k1 < u_width; k1++)
				 for (int k2 = 0; k2 < u_height; k2++)
				 {
				        double resultx, resulty;
						correspondence(k2, k1, center_xx, center_yy, &resultx, &resulty);
						int intx = resultx; 
						int inty = resulty;

						//��������ͼƬ��Χ����ֹԽ�磬��Ե�Զ���ȫ
						if (intx >= WIDTH)
							intx = WIDTH - 1;
						else if (intx < 0)
							intx = 0;
						if (inty >= HEIGHT)
							inty = HEIGHT - 1;
						else if (inty < 0)
							inty = 0;
						//if (k1 == 479 && k2 == 479) system("pause");
						u_img[i][j].at<Vec3b>(k2, k1) = img.at<Vec3b>(inty,intx);
				 }
		}


	for (int i = 0; i < range; i++)
		for (int j = 0; j < range; j++)
		{
		    bool found = findChessboardCorners(u_img[i][j], Size(ChessBoardWidth, ChessBoardHeight), corners);
		    //drawChessboardCorners(u_img[i][j], Size(ChessBoardWidth, ChessBoardHeight), corners, found);
			if (!found)
			{
				ChessScore[i][j] = 99.9999;
				ChessScore2 [i][j] = 99.9999;
				continue;
			}

			//���ߴ��
			for (int k = 0; k < ChessBoardHeight; k++)
			{
				double dis = 0;
				double sum_x = 0, sum_y = 0, avg_x = 0, avg_y, A = 0, B = 0, C = 0;
				for (int h = 0; h < ChessBoardWidth; h++)
					sum_x += corners[k*ChessBoardWidth + h].x, sum_y += corners[k*ChessBoardWidth + h].y;

				avg_x = sum_x / ChessBoardWidth;
				avg_y = sum_y / ChessBoardWidth;

				for (int h = 0; h < ChessBoardWidth; h++)
					C += (corners[k*ChessBoardWidth + h].x - avg_x)*(corners[k*ChessBoardWidth + h].x - avg_x);

				for (int h = 0; h < ChessBoardWidth; h++)
					A += (corners[k*ChessBoardWidth + h].x - avg_x)*(corners[k*ChessBoardWidth + h].y - avg_y);

				A /= C;
				B = avg_y - A*avg_x;
				for (int h = 0; h < ChessBoardWidth; h++)
					dis += fabs(A*corners[k*ChessBoardWidth + h].x - corners[k*ChessBoardWidth + h].y + B) / sqrt(A*A + 1);
				ChessScore[i][j] += dis ;
				//line(u_img[i][j], Point(0, B), Point(WIDTH*beishu, WIDTH*beishu * A + B), Scalar{ 255, 255, 0 }, 1, CV_AA);
				//for (int z = 0; z < corners.size(); z++)
				//	circle(u_img[i][j], corners[z], 3, Scalar{ 0 , 255, 0 }, 1.5);
				//cout << "Line " << k << "   Score:  " << dis << endl;
				//cout << "A:  " << A << "  B:  " << B << endl;
			}


			//����
			double AA[ChessBoardWidth], BB[ChessBoardWidth], CC[ChessBoardWidth];  //����ÿ��ֱ�ߵ�A,B,Cϵ��  y=A*x+B
			for (int k = 0; k < ChessBoardWidth; k++)
			{
				double dis = 0;
				double sum_x = 0, sum_y = 0, avg_x = 0, avg_y, A = 0, B = 0, C = 0;
				for (int h = 0; h < ChessBoardHeight; h++)
					sum_x += corners[h*ChessBoardWidth + k].x, sum_y += corners[h*ChessBoardWidth + k].y;

				avg_x = sum_x / ChessBoardHeight;
				avg_y = sum_y / ChessBoardHeight;

				for (int h = 0; h < ChessBoardHeight; h++)
					C += (corners[h*ChessBoardWidth + k].x - avg_x)*(corners[h*ChessBoardWidth + k].x - avg_x);

				for (int h = 0; h < ChessBoardHeight; h++)
					A += (corners[h*ChessBoardWidth + k].x - avg_x)*(corners[h*ChessBoardWidth + k].y - avg_y);

				A /= C;
				B = avg_y - A*avg_x;
				for (int h = 0; h < ChessBoardHeight; h++)
					dis += fabs(A*corners[h*ChessBoardWidth + k].x - corners[h*ChessBoardWidth + k].y + B) / sqrt(A*A + 1);
				ChessScore[i][j] += dis;

				AA[k] = A; BB[k] = B; CC[k] = C;
				//line(u_img[i][j], Point(-B / A, 0), Point((HEIGHT*beishu - B) / A, HEIGHT*beishu), Scalar{ 255, 255, 0 }, 1, CV_AA);
				//cout << "Line " << k << "   Score:  " << dis << endl;
				//cout << "A:  " << A << "  B:  " << B << endl;
			}


			//��ַ�ʽ�����������ܼ��̶ȴ��
			int P_num = 0;  //�������
			Point2f PP[ChessBoardHeight*ChessBoardWidth];
			for (int k = 1; k <= ChessBoardWidth; k++)
				for (int h = k + 1; h <= ChessBoardHeight; h++)
				{
				   P_num++;
				   PP[P_num] = Point((BB[h] - BB[k]) / (AA[k] - AA[h]), AA[k] * (BB[h] - BB[k]) / (AA[k] - AA[h]) + BB[k]);
				   //circle(u_img[i][j], PP[P_num], 3, Scalar{ 255, 0, 255 }, 1);
				}

			//������ĵ�
			double avg_x = 0, avg_y = 0;
			for (int k = 1; k <= P_num; k++)
			{
				avg_x += PP[k].x;
				avg_y += PP[k].y;
			}
			avg_x /= P_num; avg_y /= P_num;
			//circle(u_img[i][j], Point(avg_x, avg_y), 3, Scalar{ 0, 255, 255 }, 1);
			//���ɢϵ��
			double dis = 0;
			for (int k = 1; k <= P_num; k++)
			{
				dis += sqrt((avg_x - PP[k].x)*(avg_x - PP[k].x) + (avg_y - PP[k].y)*(avg_y - PP[k].y));
			}

			//ѡ���ַ�ʽ���Ĵ�ָ��Ƿ���
			ChessScore2[i][j] = dis / P_num;


		    //namedWindow("t11", 0);
		    //imshow("t11", u_img[i][j]);
		    //waitKey();
		}

	//�ҳ�������С�ļ�Ϊ���ŵ�
	freopen("score.txt", "w", stdout);
	double MinScore = 1000000;
	int marki = 0, markj = 0;
	for (int i = 0; i < range; i++)
		for (int j = 0; j < range; j++)
		{
		//cout << "-----------x: " << i - range / 2 << "    y: " << j - range / 2 << "-------------" << endl;
		if (ChessScore2[i][j] < MinScore)
		{
			MinScore = ChessScore2[i][j];
			marki = i;
			markj = j;
		}
		printf("%.4lf\t", ChessScore2[i][j]);
		if (j == range - 1) printf("\n");
		}
	cout << "Best Center:  x: " << center_x + marki - range / 2 << "     y: " << center_y + markj - range / 2 << endl;
	fclose(stdout);
	//cout << endl << endl;
	//cout << "Best Center:  x: " << center_x + marki - range / 2 << "     y: " << center_y+markj - range / 2 << endl;
	

	//system("pause");
	return 0;
}

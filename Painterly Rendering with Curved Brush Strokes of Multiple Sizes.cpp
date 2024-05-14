#include <opencv2/opencv.hpp>

typedef struct st {
	int length;			// ���� �� ����(path�� ����Ǵ� �� ����)
	CvScalar color;     // ���� ����
	CvPoint center;     // ������ ������
	CvPoint path[100];  // �����̴� ���
}STROKE;

// �� ������ ��� ���̸� ���ϱ�
float dist(CvScalar f, CvScalar g) {  
	return sqrt((f.val[0] - g.val[0]) * (f.val[0] - g.val[0])
			+ (f.val[1] - g.val[1]) * (f.val[1] - g.val[1])
			+ (f.val[2] - g.val[2]) * (f.val[2] - g.val[2]));
}

// ������ �׸� �׸���
void paintCircle(IplImage* dst, IplImage* ref, int R) {
	int grid = R;  // �׸��� ����
	int gx = dst->width / grid + 1;   // �׸��� ���� ����
	int gy = dst->height / grid + 1;   //  �׸��� ���� ����
	STROKE* s = (STROKE*)malloc(gx * gy * sizeof(STROKE));   // ���� ������ ����ü �迭
	STROKE st;   // temp
	int idx = 0;   // �� ����
	for (int y = 0; y < dst->height-1; y += grid) {
		for (int x = 0; x < dst->width-1; x += grid) {
			float areaerror = 0;   // ��� ����
			float maxerror = 0;    // �ִ� ����

			// ���� ����ó��
			int y1 = y + grid > dst->height ? dst->height - 1 : y + grid;
			int x1 = x + grid > dst->width ? dst->width - 1 : x + grid;

			for (int ay = y; ay < y1; ay++) {
				for (int ax = x; ax < x1; ax++) {

					CvScalar f = cvGet2D(dst, ay, ax);
					CvScalar g = cvGet2D(ref, ay, ax);
					areaerror += dist(f, g);

					/* ĵ������ ���۷��� �̹����� ���̸� ������ �迭�� ���� ������ �ʾ�
					*  areaerror�� Threshold ���ǿ� ������ �� �� ����for���� ���ƾ� �ؼ�
					*  areaerror�� ���԰� ���ÿ� �̸� maxerror�� ���ϰ� ���ǿ� �����ϴ� ��� �迭�� ���� */ 
					if (dist(f, g) > maxerror) {
						maxerror = dist(f, g);
						st.center.x = ax;
						st.center.y = ay;
						st.color = g;
					}
				}
			}
			areaerror /= (grid * grid);
			if (areaerror > 20) {   // Threshold = 20
				s[idx] = st;
				idx++;
			}
		}
	}

	// �� �������� ����
	STROKE temp;
	for (int i = 0; i < 100; i++) {
		int a = rand() % idx;
		int b = rand() % idx;
		temp = s[a];
		s[a] = s[b];
		s[b] = temp;
	}
	// �� �׸���
	for (int i = 0; i < idx; i++) {
		cvCircle(dst, s[i].center, R, s[i].color, -1);
	}

}

float gradientDirection(CvScalar f, CvScalar g) {   // �׶���Ʈ ���ϴ� �Լ�
	float d = dist(f, g);   // ��� ����
	float fv = (f.val[0] + f.val[1] + f.val[2]) / 3;   // f�� ��Ⱚ
	float gv = (g.val[0] + g.val[1] + g.val[2]) / 3;   // g�� ��Ⱚ

	// ��ȣ ����
	if (fv >= gv) return d;
	else return -d;
}

// �(curved spline) �ϳ� �׸���
STROKE makeSpline(STROKE init, IplImage* dst, IplImage* ref, int R) {
	STROKE p;
	p.length = 0;
	p.color = init.color;		 // �ʱ� ����
	p.path[0] = init.center;     // �ʱ� x,y ��
	CvPoint pre = init.center;   // ���� ��
	float lastDx = 0;   // ���� x����
	float lastDy = 0;   // ���� y ����
	int maxStrokeLength = 16;   // stroke �ִ� ����
	int minStrokeLength = 4;    // stroke �ּ� ����
	int sumR = 0, sumG = 0, sumB = 0;   // ������� �� ������ ����
	float fc = 0.5; // filter
	for (int i = 1; i < maxStrokeLength; i++) {
		CvScalar f = cvGet2D(ref, pre.y, pre.x);
		CvScalar g = cvGet2D(dst, pre.y, pre.x);
		
		sumR += f.val[2];
		sumG += f.val[1];
		sumB += f.val[0];

		if ((i > minStrokeLength) && (dist(f, g) < dist(f, p.color))) { // ĵ�������� ���̰� ���� ������ ���̺��� ���� ��
			return p;
		}

		if (pre.y - 1 < 0 || pre.y + 1 > dst->height-1 || pre.x - 1 < 0 || pre.x + 1 > dst->width-1) {  // ���� ����ó��
			return p;
		}

		CvScalar u = cvGet2D(ref, pre.y - 1, pre.x);  // up
		CvScalar d = cvGet2D(ref, pre.y + 1, pre.x);  // down
		CvScalar e = cvGet2D(ref, pre.y, pre.x - 1);  // east
		CvScalar w = cvGet2D(ref, pre.y, pre.x + 1);  // west
		float gx = gradientDirection(w, e);   // x�� ���� �׶��̾�Ʈ��
		float gy = gradientDirection(d, u);   // y�� ���� �׶��̾�Ʈ��
		float gu = sqrt(gx * gx + gy * gy);   // ������ magnitude

		if (gu == 0) {   // ������ ���̰� 0�̸�
			return p;
		}

		// ���� ���ͷ� �����
		gx /= gu;
		gy /= gu;

		// ���� ������� ������ ���� ����
		float dx = -gy;
		float dy = gx;

		// ���� ���ϱ�(���������� ������ �ٸ� �� ���� �߿� ������ ���� ������ ����)
		if (lastDx * dx + lastDy * dy < 0) {  // ����(�����̸� ���, �а��̸� ����)
			dx = -dx;
			dy = -dy;
		}

		// ���� filter
		dx = fc * dx + (1 - fc) * lastDx;
		dy = fc * dy + (1 - fc) * lastDy;
		dx /= sqrt(dx * dx + dy * dy);
		dy /= sqrt(dx * dx + dy * dy);

		// update
		pre.x += R*dx;
		pre.y += R*dy;
		lastDx = dx;
		lastDy = dy;
		p.path[i] = pre;
		p.length++;

		if (pre.y < 0 || pre.y > dst->height-1 || pre.x < 0 || pre.x > dst->width-1) {   // ���� ����ó��
			return p;
		}

		// ��� ���� (�ʱ� �������� �׸��� �׸��� ��ĥ�� ǥ����)
		p.color.val[2] = sumR / i;
		p.color.val[1] = sumG / i;
		p.color.val[0] = sumB / i;
	}

	return p;
}

// ����� �׸� �׸���(curved spline brush)
void paintSpline(IplImage* dst, IplImage* ref, int R) {   // paintCircle�Լ��� ����
	int grid = 1.2*R;   // �׸��� ����
	int gx = dst->width / grid + 1;   // �׸��� ���� ����
	int gy = dst->height / grid + 1;  // �׸��� ���� ����
	STROKE* k = (STROKE*)malloc(gx * gy * sizeof(STROKE));   // ���� ������ ����ü �迭
	STROKE kt;  // temp
	int idx = 0;
	for (int y = 0; y < dst->height - 1; y += grid) {
		for (int x = 0; x < dst->width - 1; x += grid) {
			float areaerror = 0;
			float maxerror = 0;
			int y1 = y + grid > dst->height ? dst->height - 1 : y + grid;
			int x1 = x + grid > dst->width ? dst->width - 1 : x + grid;
			for (int ay = y; ay < y1; ay++) {
				for (int ax = x; ax < x1; ax++) {

					CvScalar f = cvGet2D(dst, ay, ax);
					CvScalar g = cvGet2D(ref, ay, ax);
					areaerror += dist(f, g);
					if (dist(f, g) > maxerror) {
						maxerror = dist(f, g);
						kt.center.x = ax;
						kt.center.y = ay;
						kt.color = g;
					}
				}
			}
			areaerror /= (grid * grid);
			if (areaerror >	20) {   // Threshold = 20
				k[idx] = makeSpline(kt, dst, ref, R);
				idx++;
			}
		}
	}

	// �� �׸���
	for (int i = 0; i < idx; i++) {
		for (int j = 0; j < k[i].length; j++) {
			cvLine(dst, k[i].path[j], k[i].path[j + 1], k[i].color, R);
		}
	}

}

int main() {

	int mode;
	char str[100];
	printf("Input File Path : ");
	scanf("%s", str);
	printf("Select Drawing Mode (0=circle, 1=stroke) : ");
	scanf("%d", &mode);
	
	IplImage* src = cvLoadImage(str);   // �ҽ� �̹���
	IplImage* ref = cvCreateImage(cvGetSize(src), 8, 3);   // ���۷��� �̹���
	IplImage* dst = cvCreateImage(cvGetSize(src), 8, 3);   // ĵ����
	cvSet(dst, cvScalar(255, 255, 255));   // ĵ���� �Ͼ�� ĥ�ϱ�

	int stroke[5] = { 35,19,15,7,3 };   // �귯�� ũ��
	for (int i = 0; i < 5; i++) {
		cvSmooth(src, ref, CV_GAUSSIAN, stroke[i]*2+1);  // ���۷��� �̹��� ����

		switch (mode) {
		case 0:		paintCircle(dst, ref, stroke[i]);	break;
		case 1:		paintSpline(dst, ref, stroke[i]);	break;
		default:										break;
		}

		cvShowImage("dst", dst);
		cvWaitKey(1000);
	}

	cvShowImage("dst", dst);
	cvWaitKey();

	return 0;
}
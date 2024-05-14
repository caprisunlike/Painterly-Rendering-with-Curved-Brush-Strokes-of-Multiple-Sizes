#include <opencv2/opencv.hpp>

typedef struct st {
	int length;			// 선의 총 길이(path에 저장되는 점 개수)
	CvScalar color;     // 붓의 색깔
	CvPoint center;     // 붓질의 시작점
	CvPoint path[100];  // 움직이는 경로
}STROKE;

// 두 색깔의 밝기 차이를 구하기
float dist(CvScalar f, CvScalar g) {  
	return sqrt((f.val[0] - g.val[0]) * (f.val[0] - g.val[0])
			+ (f.val[1] - g.val[1]) * (f.val[1] - g.val[1])
			+ (f.val[2] - g.val[2]) * (f.val[2] - g.val[2]));
}

// 점으로 그림 그리기
void paintCircle(IplImage* dst, IplImage* ref, int R) {
	int grid = R;  // 그리드 길이
	int gx = dst->width / grid + 1;   // 그리드 가로 개수
	int gy = dst->height / grid + 1;   //  그리드 세로 개수
	STROKE* s = (STROKE*)malloc(gx * gy * sizeof(STROKE));   // 점을 저장할 구조체 배열
	STROKE st;   // temp
	int idx = 0;   // 점 개수
	for (int y = 0; y < dst->height-1; y += grid) {
		for (int x = 0; x < dst->width-1; x += grid) {
			float areaerror = 0;   // 평균 에러
			float maxerror = 0;    // 최대 에러

			// 범위 예외처리
			int y1 = y + grid > dst->height ? dst->height - 1 : y + grid;
			int x1 = x + grid > dst->width ? dst->width - 1 : x + grid;

			for (int ay = y; ay < y1; ay++) {
				for (int ax = x; ax < x1; ax++) {

					CvScalar f = cvGet2D(dst, ay, ax);
					CvScalar g = cvGet2D(ref, ay, ax);
					areaerror += dist(f, g);

					/* 캔버스와 래퍼런스 이미지의 차이를 저장한 배열을 따로 만들지 않아
					*  areaerror가 Threshold 조건에 성립할 때 또 이중for문을 돌아야 해서
					*  areaerror를 구함과 동시에 미리 maxerror를 구하고 조건에 성립하는 경우 배열에 저장 */ 
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

	// 점 랜덤으로 섞기
	STROKE temp;
	for (int i = 0; i < 100; i++) {
		int a = rand() % idx;
		int b = rand() % idx;
		temp = s[a];
		s[a] = s[b];
		s[b] = temp;
	}
	// 점 그리기
	for (int i = 0; i < idx; i++) {
		cvCircle(dst, s[i].center, R, s[i].color, -1);
	}

}

float gradientDirection(CvScalar f, CvScalar g) {   // 그라디언트 구하는 함수
	float d = dist(f, g);   // 밝기 차이
	float fv = (f.val[0] + f.val[1] + f.val[2]) / 3;   // f의 밝기값
	float gv = (g.val[0] + g.val[1] + g.val[2]) / 3;   // g의 밝기값

	// 부호 결정
	if (fv >= gv) return d;
	else return -d;
}

// 곡선(curved spline) 하나 그리기
STROKE makeSpline(STROKE init, IplImage* dst, IplImage* ref, int R) {
	STROKE p;
	p.length = 0;
	p.color = init.color;		 // 초기 색상
	p.path[0] = init.center;     // 초기 x,y 값
	CvPoint pre = init.center;   // 이전 점
	float lastDx = 0;   // 이전 x방향
	float lastDy = 0;   // 이전 y 방향
	int maxStrokeLength = 16;   // stroke 최대 길이
	int minStrokeLength = 4;    // stroke 최소 길이
	int sumR = 0, sumG = 0, sumB = 0;   // 색상들의 합 저장할 변수
	float fc = 0.5; // filter
	for (int i = 1; i < maxStrokeLength; i++) {
		CvScalar f = cvGet2D(ref, pre.y, pre.x);
		CvScalar g = cvGet2D(dst, pre.y, pre.x);
		
		sumR += f.val[2];
		sumG += f.val[1];
		sumB += f.val[0];

		if ((i > minStrokeLength) && (dist(f, g) < dist(f, p.color))) { // 캔버스와의 차이가 현재 색과의 차이보다 작을 때
			return p;
		}

		if (pre.y - 1 < 0 || pre.y + 1 > dst->height-1 || pre.x - 1 < 0 || pre.x + 1 > dst->width-1) {  // 범위 예외처리
			return p;
		}

		CvScalar u = cvGet2D(ref, pre.y - 1, pre.x);  // up
		CvScalar d = cvGet2D(ref, pre.y + 1, pre.x);  // down
		CvScalar e = cvGet2D(ref, pre.y, pre.x - 1);  // east
		CvScalar w = cvGet2D(ref, pre.y, pre.x + 1);  // west
		float gx = gradientDirection(w, e);   // x축 방향 그라이언트값
		float gy = gradientDirection(d, u);   // y축 방향 그라이언트값
		float gu = sqrt(gx * gx + gy * gy);   // 벡터의 magnitude

		if (gu == 0) {   // 벡터의 길이가 0이면
			return p;
		}

		// 단위 벡터로 만들기
		gx /= gu;
		gy /= gu;

		// 색이 밝아지는 방향의 수직 방향
		float dx = -gy;
		float dy = gx;

		// 방향 정하기(평행하지만 방향은 다른 두 벡터 중에 각도가 작은 것으로 선택)
		if (lastDx * dx + lastDy * dy < 0) {  // 내적(예각이면 양수, 둔각이면 음수)
			dx = -dx;
			dy = -dy;
		}

		// 방향 filter
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

		if (pre.y < 0 || pre.y > dst->height-1 || pre.x < 0 || pre.x > dst->width-1) {   // 범위 예외처리
			return p;
		}

		// 평균 색상 (초기 색상으로 그리니 그림이 거칠게 표현됨)
		p.color.val[2] = sumR / i;
		p.color.val[1] = sumG / i;
		p.color.val[0] = sumB / i;
	}

	return p;
}

// 곡선으로 그림 그리기(curved spline brush)
void paintSpline(IplImage* dst, IplImage* ref, int R) {   // paintCircle함수와 유사
	int grid = 1.2*R;   // 그리드 길이
	int gx = dst->width / grid + 1;   // 그리드 가로 개수
	int gy = dst->height / grid + 1;  // 그리드 세로 개수
	STROKE* k = (STROKE*)malloc(gx * gy * sizeof(STROKE));   // 선을 저장할 구조체 배열
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

	// 선 그리기
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
	
	IplImage* src = cvLoadImage(str);   // 소스 이미지
	IplImage* ref = cvCreateImage(cvGetSize(src), 8, 3);   // 래퍼런스 이미지
	IplImage* dst = cvCreateImage(cvGetSize(src), 8, 3);   // 캔버스
	cvSet(dst, cvScalar(255, 255, 255));   // 캔버스 하얗게 칠하기

	int stroke[5] = { 35,19,15,7,3 };   // 브러쉬 크기
	for (int i = 0; i < 5; i++) {
		cvSmooth(src, ref, CV_GAUSSIAN, stroke[i]*2+1);  // 래퍼런스 이미지 생성

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
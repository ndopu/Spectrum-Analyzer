/*
 * fft.h
 *
 * Created: 2019/05/02 14:08:03
 *  Author: Linear M. Kayavala
 */ 


#ifndef FFT_H_
#define FFT_H_

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

typedef struct {
	float real, imag;
} cmplx;

cmplx cmplx_mul_add(const cmplx c, const cmplx a, const cmplx b) {
	const cmplx ret = {
		(a.real * b.real) + c.real - (a.imag * b.imag),
		(a.imag * b.real) + (a.real * b.imag) + c.imag
	};
	return ret;
}

void fft_Stockham(const cmplx *input, cmplx *output, int n, int flag) {
	int half = n >> 1;
	cmplx *buffer = (cmplx *) calloc(sizeof(cmplx), n * 2);
	if (buffer == NULL)
	return;
	cmplx *tmp = buffer;
	cmplx *y = tmp + n;
	memcpy(y, input, sizeof(cmplx) * n);
	for (int r = half, l = 1; r >= 1; r >>= 1) {
		cmplx *tp = y;
		y = tmp;
		tmp = tp;
		float factor_w = -flag * M_PI / l;
		cmplx w = {cosf(factor_w), sinf(factor_w)};
		cmplx wj = {1, 0};
		for (int j = 0; j < l; j++) {
			int jrs = j * (r << 1);
			for (int k = jrs, m = jrs >> 1; k < jrs + r; k++) {
				const cmplx t = {(wj.real * tmp[k + r].real) - (wj.imag * tmp[k + r].imag),
				(wj.imag * tmp[k + r].real) + (wj.real * tmp[k + r].imag)};
				y[m].real = tmp[k].real + t.real;
				y[m].imag = tmp[k].imag + t.imag;
				y[m + half].real = tmp[k].real - t.real;
				y[m + half].imag = tmp[k].imag - t.imag;
				m++;
			}
			const float t = wj.real;
			wj.real = (t * w.real) - (wj.imag * w.imag);
			wj.imag = (wj.imag * w.real) + (t * w.imag);
		}
		l <<= 1;
	}
	memcpy(output, y, sizeof(cmplx) * n);
	free(buffer);
}

int base(int n) {
	int t = n & (n - 1);
	if (t == 0) {
		return 2;
	}
	for (int i = 3; i <= 7; i++) {
		int n2 = n;
		while (n2 % i == 0) {
			n2 /= i;
		}
		if (n2 == 1) {
			return i;
		}
	}
	return n;
}




#endif /* FFT_H_ */
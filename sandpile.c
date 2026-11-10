#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static void die(const char *s) {
  fprintf(stderr, "%s\n", s);
  exit(1);
}

int N = 801;
//int **grid;

int16_t *grid, *grid_next;
int W;
int *pac_low, *pac_hi;
int *pad_low, *pad_hi;
int *win_low, *win_hi;

#define GRID(i, j) grid[(size_t)((i)+1) * W + ((j) + 1)]

static void compute_boundary(void) {
	pac_low = (int*)malloc(sizeof(int) * N);
	pac_hi  = (int*)malloc(sizeof(int) * N);
	int r = N/2;
	for (int i = 0; i < N; i++) {
		pac_low[i] = 0;
		pac_hi[i] = -1;
		for (int j = 0; j <= MAX(i, N-1-i); j++) {
			if ((i - r) * (i - r) + (j - r) * (j - r) <= r * r) {
				if (pac_hi[i] < 0) {
					pac_low[i] = j;
				}
				pac_hi[i] = j;
			}
		}
	}
}

static void compute_window(void) {
	pad_low = (int*)malloc(sizeof(int) * (N + 2));
	pad_hi  = (int*)malloc(sizeof(int) * (N + 2));
	win_low = (int*)malloc(sizeof(int) * N);
	win_hi  = (int*)malloc(sizeof(int) * N);
	if (!pad_low || !pad_hi || !win_low || !win_hi)
		die("malloc failed for row windows");
	pad_low[0] = pad_low[N + 1] = 1;
	pad_hi[0]  = pad_hi[N + 1]  = 0;
	for (int i = 0; i < N; i++) {
		pad_low[i + 1] = pac_low[i];
		pad_hi[i + 1]  = pac_hi[i];
	}
	for (int i = 0; i < N; i++) {
		int lo = N, hi = -1;
		for (int k = i - 1; k <= i + 1; k++) { 
			int l = pad_low[k + 1], r = pad_hi[k + 1];
			if (l > r)
				continue;
			if (k == i) { 
				l -= 1;
				r += 1;
			}
			lo = MIN(lo, l);
			hi = MAX(hi, r);
		}
		win_low[i] = MAX(lo, 0);
		win_hi[i]  = MIN(hi, N - 1);
	}
}

// you can also use this function to debug print your sandpile
static void render(char *filename) {
  int chan = 3;
  unsigned char *pixels = (unsigned char *)malloc(N * N * chan * sizeof(char));
  int stride = N * chan;
  if (!pixels)
    die("malloc failed for PNG pixel buffer");
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      unsigned char r = 0, g = 0, b = 0;
      switch (GRID(i, j)) {
      case 0: // obsidian
        r = 15;
        g = 15;
        b = 25;
        break;
      case 1: // electric cyan
        r = 0;
        g = 210;
        b = 255;
        break;
      case 2: // vibrant magenta
        r = 230;
        g = 30;
        b = 130;
        break;
      case 3: // bright gold
        r = 255;
        g = 200;
        b = 0;
        break;
      default: // white
        r = 0;
        g = 0;
        b = 0;
      }

      int pid = (i * N + j) * chan;
      pixels[pid] = r;
      pixels[pid + 1] = g;
      pixels[pid + 2] = b;
    }
  }
  if (stbi_write_png(filename, N, N, chan, pixels, stride)) {
    printf("Successfully saved sandpile identity to: %s\n", filename);
  } else {
    die("Error: stb_image_write failed to save png file\n");
  }
  free(pixels);
}

static void stabilize(void) {
  const int n = N, w = W;
  const int sym = n % 2, h = n / 2;
  const int rows = sym ? h + 1 : n;   
  memcpy(grid_next, grid, (size_t)(n + 2) * w * sizeof(int16_t));
  int changed;
  #pragma omp parallel
  {
    const int16_t *src = grid;
    int16_t *dst = grid_next;
    int tchanged;
    do {
      #pragma omp single   
      changed = 0;
      #pragma omp for schedule(static, 4) reduction(|:changed)
      for (int i = 0; i < rows; ++i) {
	const int d = (sym && i == h) ? i - 1 : i + 1;
        const int16_t *restrict up = src + (size_t)i * w + 1;   
        const int16_t *restrict cur = up + w;                 
        const int16_t *restrict dn = src + (size_t)(d + 1) * w + 1;  //dn is just up for the middle row
        int16_t *restrict out = dst + (size_t)(i + 1) * w + 1; 

	const int up_lo = pad_low[i], up_hi = pad_hi[i];
        const int lo = pad_low[i + 1], hi = pad_hi[i + 1];
        const int dn_lo = pad_low[d + 1], dn_hi = pad_hi[d + 1];
        const int jl = win_low[i], jh = win_hi[i];
        int any = 0;
        for (int j = jl; j <= jh; ++j) {
          int16_t qu = (j >= up_lo && j <= up_hi) ? up[j] >> 2 : 0;
          int16_t qd = (j >= dn_lo && j <= dn_hi) ? dn[j] >> 2 : 0;
          int16_t ql = (j - 1 >= lo && j - 1 <= hi) ? cur[j - 1] >> 2 : 0;
          int16_t qr = (j + 1 >= lo && j + 1 <= hi) ? cur[j + 1] >> 2 : 0;
          int16_t qc = (j >= lo && j <= hi) ? cur[j] >> 2 : 0;
          out[j] = cur[j] - 4 * qc + qu + qd + ql + qr;
          any |= qc;
        }
        changed |= any;
      }   
      tchanged = changed;
      int16_t *tmp = (int16_t*)src;
      src = dst;
      dst = tmp;
      #pragma omp barrier
    } while (tchanged);
    #pragma omp single
    {
      grid = (int16_t *)src; //fix the double buffer flip floppin
      grid_next = dst;
    }
  }
  if (sym)
    for (int i = h + 1; i < n; ++i)
      memcpy(&GRID(i, 0), &GRID(n - 1 - i, 0), n * sizeof(int16_t));
}

void find_identity() {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      GRID(i, j) = 6;
    }
  }
  stabilize();
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      GRID(i, j) = 6 - GRID(i, j);
    }
  }
  stabilize();
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 2) {
    fprintf(stderr, "Usage: %s N\n", argv[0]);
    return EXIT_FAILURE;
  }
  char *end;
  uintmax_t parsed = strtoumax(argv[1], &end, 10);
  if (errno || argv[1][0] < '0' || argv[1][0] > '9' || *end || !parsed ||
      parsed > INT32_MAX)
    die("grid size must be a positive int");
  N = (int)parsed;
  
  W = N + 2;
  grid = (int16_t *)calloc((size_t)(N + 2) * W, sizeof(int16_t));
  grid_next = (int16_t *)calloc((size_t)(N + 2) * W, sizeof(int16_t));


  compute_boundary();
  compute_window();

  find_identity();

  render("identity.png");
  free(grid);
  free(grid_next);
  free(pac_low);
  free(pac_hi);
  free(pad_low);
  free(pad_hi);
  free(win_low);
  free(win_hi);

  return EXIT_SUCCESS;
}

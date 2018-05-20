/*
  SHIRO
  ===
  Copyright (c) 2017-2018 Kanru Hua. All rights reserved.

  This file is part of SHIRO.

  SHIRO is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  SHIRO is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SHIRO.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "external/ciglet/ciglet.h"

char* mystrdup(const char *str) {
  int n = strlen(str) + 1;
  char* ret = malloc(n);
  strcpy(ret, str);
  return ret;
}

static void write_float_data(FP_TYPE* x, int nx) {
  float* xfloat = calloc(nx, sizeof(float));
  for(int i = 0; i < nx; i ++) xfloat[i] = x[i];
  fwrite(xfloat, 4, nx, stdout);
  free(xfloat);
}

static FP_TYPE* read_float_data(const char* path, int* nx) {
  FILE* fin = NULL;
  if(! strcmp(path, "-"))
    fin = stdin;
  else
    fin = fopen(path, "rb");
  fseek(fin, 0, SEEK_END);
  int fsize = ftell(fin);
  fseek(fin, 0, SEEK_SET);
  *nx = fsize / sizeof(float);
  
  float* retf = malloc(fsize);
  fread(retf, sizeof(float), *nx, fin);
  FP_TYPE* retfp = malloc(*nx * sizeof(FP_TYPE));
  for(int i = 0; i < *nx; i ++) retfp[i] = retf[i];
  free(retf);

  if(strcmp(path, "-"))
    fclose(fin);
  return retfp;
}

static void compute_dynamic_feature(FP_TYPE** fstatic, int nfrm, int size,
  FP_TYPE* d, int nd, FP_TYPE** dst, int idx) {
  int halfnd = nd / 2;
  for(int i = 0; i < nfrm; i ++) {
    for(int j = 0; j < size; j ++) {
      FP_TYPE sum = 0;
      for(int k = -halfnd; k <= halfnd; k ++)
        if(i + k >= 0 && i + k < nfrm)
          sum += fstatic[i + k][j] * d[k + halfnd];
      dst[i][j + idx] = sum;
    }
  }
}

static void print_usage() {
  fprintf(stderr,
    "shiro-xxcc path-to-raw-file\n"
    "  -f feature-type (mfcc or plpcc)\n"
    "  -m order\n"
    "  -c number-of-channels\n"
    "  -l frame-length\n"
    "  -p hop-size\n"
    "  -s sample-rate (in kHz)\n"
    "  -d (include dynamic feature)\n"
    "  -a (include 2nd-order dynamic feature)\n"
    "  -e (include energy, if applicable)\n"
    "  -0 (include 0-th DCT coefficient)\n"
    "  -E energy-type \n"
    "  -h (print usage)\n"
    "energy-type\n"
    "   0 RMS energy\n"
    "   1 RMS energy (dB)\n");
  exit(1);
}

char* input_raw = NULL;
char* opt_featuretype = NULL;
int   opt_order = 12;
int   opt_nchannel = 36;
int   opt_framesize = 1024;
FP_TYPE opt_hopsize = 256;
FP_TYPE opt_fs = 32000;
int   opt_d = 0;
int   opt_a = 0;
int   opt_0 = 0;
int   opt_e = 0;
int   opt_E = 0;

static void main_xxcc() {
  int nstatic = opt_order + opt_e + opt_0;
  int nparam = nstatic * (1 + opt_d + opt_a);
  
  int nx = 0;
  FP_TYPE* x = read_float_data(input_raw, & nx);

  int nfft = pow(2, ceil(log2(opt_framesize)));
  filterbank* fb = NULL;
  if(! strcmp(opt_featuretype, "mfcc"))
    fb = create_melfilterbank(nfft / 2 + 1, opt_fs / 2, opt_nchannel,
      50, opt_fs / 2);
  else if(! strcmp(opt_featuretype, "plpcc"))
    fb = create_plpfilterbank(nfft / 2 + 1, opt_fs / 2, opt_nchannel);
  else {
    fprintf(stderr, "Error: undefined feature type \"%s\"\n", opt_featuretype);
    exit(1);
  }

  // filtering and DCT

  int nfrm = nx / opt_hopsize;
  FP_TYPE** Be = calloc(nfrm, sizeof(FP_TYPE*));
  FP_TYPE* energy = calloc(nfrm, sizeof(FP_TYPE));
  FP_TYPE* w = blackman(opt_framesize);
  for(int i = 0; i < nfrm; i ++) {
    int center = opt_hopsize * i;
    FP_TYPE* xfrm = fetch_frame(x, nx, center, opt_framesize);
    FP_TYPE* fftbuff = calloc(nfft * 4, sizeof(FP_TYPE));
    FP_TYPE* x_re = fftbuff; FP_TYPE* x_im = fftbuff + nfft;
    for(int j = 0; j < opt_framesize; j ++) {
      x_re[j] = xfrm[j] * w[j];
      energy[i] += xfrm[j] * xfrm[j] * w[j];
    }
    energy[i] = sqrt(energy[i] / opt_framesize);
    fft(x_re, NULL, x_re, x_im, nfft, fftbuff + nfft * 2);
    for(int j = 0; j < nfft; j ++)
      x_re[j] = sqrt(x_re[j] * x_re[j] + x_im[j] * x_im[j]);
    if(! strcmp(opt_featuretype, "plpcc"))
      Be[i] = filterbank_spec(fb, x_re, nfft, opt_fs, 1);
    else
      Be[i] = filterbank_spec(fb, x_re, nfft, opt_fs, 0);
    for(int j = 0; j < opt_nchannel; j ++)
      Be[i][j] = max(-15.0, Be[i][j]);
    free(fftbuff);
    free(xfrm);
  }

  FP_TYPE** C = calloc(nfrm, sizeof(FP_TYPE*));
  for(int i = 0; i < nfrm; i ++) {
    C[i] = be2cc(Be[i], opt_nchannel, opt_order, opt_0);
    if(opt_e) {
      C[i] = realloc(C[i], nstatic * sizeof(FP_TYPE));
      C[i][nstatic - 1] = opt_E == 0 ? energy[i] : 20 * log10(energy[i]);
    }
  }

  free2d(Be, nfrm);
  free(energy);

  // differentiation

  FP_TYPE** F = malloc2d(nfrm, nparam, sizeof(FP_TYPE));
  FP_TYPE d0[1] = {1.0};
  int fidx = 0;

  compute_dynamic_feature(C, nfrm, nstatic, d0, 1, F, fidx);
  fidx += nstatic;
  if(opt_d) {
    FP_TYPE d1[3] = {-0.5, 0, 0.5};
    compute_dynamic_feature(C, nfrm, nstatic, d1, 3, F, fidx);
    fidx += nstatic;
  }
  if(opt_a) {
    FP_TYPE d2[5] = {0.25, 0, -0.5, 0, 0.25};
    compute_dynamic_feature(C, nfrm, nstatic, d2, 5, F, fidx);
  }

  // output

  FP_TYPE* Fout = flatten(F, nfrm, nparam, sizeof(FP_TYPE));
  write_float_data(Fout, nfrm * nparam);
  free(Fout);

  free2d(F, nfrm);
  free2d(C, nfrm);

  delete_filterbank(fb);
  free(x);
}

extern char* optarg;
int main(int argc, char** argv) {
# ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
# endif
  int c;
  opt_featuretype = mystrdup("mfcc");

  while((c = getopt(argc, argv, "f:m:c:l:p:s:da0eE:h")) != -1) {
    switch(c) {
    case 'f':
      free(opt_featuretype);
      opt_featuretype = mystrdup(optarg);
    break;
    case 'm':
      opt_order = atoi(optarg);
      if(opt_order < 1) {
        fprintf(stderr, "Error: invalid feature order.\n");
        exit(1);
      }
    break;
    case 'c':
      opt_nchannel = atoi(optarg);
      if(opt_nchannel < 1) {
        fprintf(stderr, "Error: invalid number of channels.\n");
        exit(1);
      }
    break;
    case 'l':
      opt_framesize = atoi(optarg);
      if(opt_framesize < 1) {
        fprintf(stderr, "Error: invalid frame size.\n");
        exit(1);
      }
    break;
    case 'p':
      opt_hopsize = atof(optarg);
      if(opt_hopsize < 1) {
        fprintf(stderr, "Error: invalid hop size.\n");
        exit(1);
      }
    break;
    case 's':
      opt_fs = atof(optarg) * 1000;
      if(opt_order <= 0) {
        fprintf(stderr, "Error: invalid sample rate.\n");
        exit(1);
      }
    break;
    case 'd':
      opt_d = 1;
    break;
    case 'a':
      opt_a = 1;
    break;
    case '0':
      opt_0 = 1;
    break;
    case 'e':
      opt_e = 1;
    break;
    case 'E':
      opt_E = atoi(optarg);
    break;
    case 'h':
      print_usage();
    break;
    default:
      abort();
    }
  }
  opt_nchannel = max(opt_nchannel, opt_order + 1);

  if(optind >= argc) {
    input_raw = mystrdup("-");
  } else
    input_raw = mystrdup(argv[optind]);

  main_xxcc();

  free(opt_featuretype);
  free(input_raw);
  return 0;
}

/*
  SHIRO
  ===
  Copyright (c) 2017 Kanru Hua. All rights reserved.

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
    "  -h (print usage)\n");
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
int   opt_e = 0;

static void main_xxcc() {
  int nstatic = opt_order + opt_e;
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

  // spectrogram analysis

  int nfrm = nx / opt_hopsize;
  int* naxis = calloc(nfrm, sizeof(int));
  int* nwin = calloc(nfrm, sizeof(int));
  for(int i = 0; i < nfrm; i ++) {
    naxis[i] = opt_hopsize * i;
    nwin[i] = opt_framesize;
  }

  FP_TYPE** S = malloc2d(nfrm, nfft / 2 + 1, sizeof(FP_TYPE));
  FP_TYPE norm_factor, weight_factor;
  cig_stft_forward(x, nx, naxis, nwin, nfrm, nfft, "blackman", 0, 2,
    & norm_factor, & weight_factor, S, NULL);

  // filtering and DCT

  FP_TYPE** Be = NULL;
  if(! strcmp(opt_featuretype, "mfcc"))
    Be = filterbank_spgm(fb, S, nfrm, nfft, opt_fs, 0);
  else if(! strcmp(opt_featuretype, "plpcc"))
    Be = filterbank_spgm(fb, S, nfrm, nfft, opt_fs, 1);

  FP_TYPE** C = calloc(nfrm, sizeof(FP_TYPE*));
  for(int i = 0; i < nfrm; i ++)
    C[i] = be2cc(Be[i], opt_nchannel, opt_order, opt_e);

  free2d(S, nfrm);
  free(naxis); free(nwin);
  free2d(Be, nfrm);

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
  int c;
  opt_featuretype = mystrdup("mfcc");

  while((c = getopt(argc, argv, "f:m:c:l:p:s:daeh")) != -1) {
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
    case 'e':
      opt_e = 1;
    break;
    case 'h':
      print_usage();
    break;
    default:
      abort();
    }
  }

  if(optind >= argc) {
    input_raw = mystrdup("-");
  } else
    input_raw = mystrdup(argv[optind]);

  main_xxcc();

  free(opt_featuretype);
  free(input_raw);
  return 0;
}

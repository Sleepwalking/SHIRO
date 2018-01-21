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
#include "external/cJSON/cJSON.h"
#include "external/liblrhsmm/common.h"
#include "external/liblrhsmm/estimate.h"
#include "external/liblrhsmm/serial.h"
#include <omp.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "cli-common.h"

static void print_usage() {
  fprintf(stderr,
    "shiro-rest\n"
    "  -m model-file\n"
    "  -s segmentation-file\n"
    "  -n num-iteration\n"
    "  -g (treat model as HMM)\n"
    "  -p state-level-pruning (HSMM)\n"
    "  -P state-level-pruning (HMM)\n"
    "  -d extra-duration-search-space\n"
    "  -t termination-threshold\n"
    "  -i (isolated training)\n"
    "  -D (DAEM training)\n"
    "  -T (enable multi-threading)\n"
    "  -h (print usage)\n");
  exit(1);
}

int opt_niter = 1;
int opt_geodur = 0;
int opt_daem = 0;
int opt_mthread = 0;
int opt_embdtrain = 1;

FP_TYPE reestimate(lrh_model_stat* hstat, lrh_model* hsmm, lrh_observ* o,
  cJSON* j_states) {
  FP_TYPE lh = 0;
  if(! opt_embdtrain) {
    lrh_dataset* d = load_isolated_data_from_json(j_states, o);
    int nsample = d -> observset -> nsample;
    for(int e = 0; e < nsample; e ++) {
      lrh_seg* es = d -> segset -> samples[e];
      lrh_observ* eo = d -> observset -> samples[e];
      for(int i = 0; i < es -> nseg; i ++)
        if(es -> time[i] > eo -> nt)
          es -> time[i] = eo -> nt;
      lrh_seg_buildjumps(es);
      if(opt_geodur)
        lh += lrh_estimate_geometric(hstat, hsmm, eo, es) / nsample;
      else
        lh += lrh_estimate(hstat, hsmm, eo, es) / nsample;
    }
    delete_dataset(d);
  } else {
    lrh_seg* s = load_seg_from_json(j_states, hsmm -> nstream);
    for(int i = 0; i < s -> nseg; i ++)
      if(s -> time[i] > o -> nt)
        s -> time[i] = o -> nt;
    lrh_seg_buildjumps(s);
    if(opt_geodur)
      lh += lrh_estimate_geometric(hstat, hsmm, o, s);
    else
      lh += lrh_estimate(hstat, hsmm, o, s);
    lrh_delete_seg(s);
  }
  return lh;
}

extern char* optarg;
int main(int argc, char** argv) {
# ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
# endif
  int c;
  cJSON* j_segm = NULL;
  lrh_model* hsmm = NULL;

  FP_TYPE opt_threshold = 1.0;
  while((c = getopt(argc, argv, "m:s:n:gp:P:d:t:iDTh")) != -1) {
    char* jsonstr = NULL;
    switch(c) {
    case 'm':
      hsmm = load_model(optarg);
      if(hsmm == NULL) {
        fprintf(stderr, "Error: failed to load model from %s\n", optarg);
        return 1;
      }
    break;
    case 's':
      jsonstr = readall(optarg);
      if(jsonstr == NULL) {
        fprintf(stderr, "Error: cannot open %s.\n", optarg);
        return 1;
      }
      j_segm = cJSON_Parse(jsonstr);
      if(j_segm == NULL) {
        fprintf(stderr, "Error: failed to parse %s.\n", optarg);
        return 1;
      }
      free(jsonstr);
    break;
    case 'n':
      opt_niter = atoi(optarg);
    break;
    case 'g':
      opt_geodur = 1;
    break;
    case 'p':
      lrh_inference_stprune = atoi(optarg);
    break;
    case 'P':
      lrh_inference_stprune_full_slope = atoi(optarg);
    break;
    case 'd':
      lrh_inference_duration_extra = atoi(optarg);
    break;
    case 't':
      opt_threshold = atof(optarg);
    break;
    case 'i':
      opt_embdtrain = 0;
    break;
    case 'D':
      opt_daem = 1;
    break;
    case 'T':
      opt_mthread = 1;
    break;
    case 'h':
      print_usage();
    break;
    default:
      abort();
    }
  }
  if(j_segm == NULL) {
    fprintf(stderr, "Error: segmentation file is not specified.\n");
    return 1;
  }
  if(hsmm == NULL) {
    fprintf(stderr, "Error: model file is not specified.\n");
    return 1;
  }
# ifdef _OPENMP
  if(opt_mthread == 0)
    omp_set_num_threads(1);
# endif
# ifndef _OPENMP
  if(opt_mthread == 1)
    fprintf(stderr, "Warning: OpenMP is not supported by this build.\n");
# endif

  cJSON* j_file_list = cJSON_GetObjectItem(j_segm, "file_list");
  checkvar(file_list);
  int nfile = cJSON_GetArraySize(j_file_list);

  FP_TYPE prev_lh = 0;
  for(int iter = 0; iter < opt_niter; iter ++) {
    if(opt_daem) {
      lrh_daem_temperature = sqrt((FP_TYPE)(iter + 1) / opt_niter);
      fprintf(stderr, "Running iteration %d/%d, temperature = %.2f...\n",
        iter, opt_niter, lrh_daem_temperature);
    } else {
      fprintf(stderr, "Running iteration %d/%d...\n", iter, opt_niter);
    }

    FP_TYPE total_lh = 0;

    lrh_model_stat* hstat = lrh_model_stat_from_model(hsmm);
    lrh_model_precompute(hsmm);
#   pragma omp parallel for
    for(int f = 0; f < nfile; f ++) {
      cJSON* j_file_list_f = cJSON_GetArrayItem(j_file_list, f);
      cJSON* j_filename = cJSON_GetObjectItem(j_file_list_f, "filename");
      checkvar(filename);
      cJSON* j_states = cJSON_GetObjectItem(j_file_list_f, "states");
      checkvar(states);
      
      lrh_observ* o = load_observ_from_float(j_filename -> valuestring, hsmm);
      total_lh += reestimate(hstat, hsmm, o, j_states);
      lrh_delete_observ(o);
    }

    if(opt_geodur)
      lrh_model_update(hsmm, hstat, 1);
    else
      lrh_model_update(hsmm, hstat, 0);
    lrh_delete_model_stat(hstat);

    FP_TYPE mean_lh = total_lh / nfile / lrh_daem_temperature;
    fprintf(stderr, "Average log likelihood = %f.\n", mean_lh);
    if(iter > 0 && opt_threshold > 0 && mean_lh < prev_lh + opt_threshold) {
      fprintf(stderr, "Training converged.\n");
      break;
    }
    prev_lh = mean_lh;
  }

  cmp_ctx_t cmpobj;
  cmp_init(& cmpobj, stdout, file_reader, file_writer);
  lrh_write_model(& cmpobj, hsmm);

  cJSON_Delete(j_segm);
  lrh_delete_model(hsmm);
  return 0;
}

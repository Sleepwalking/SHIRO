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
#include "external/liblrhsmm/inference.h"
#include "external/liblrhsmm/serial.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "cli-common.h"

static void print_usage() {
  fprintf(stderr,
    "shiro-align\n"
    "  -m model-file\n"
    "  -s segmentation-file\n"
    "  -g (use geometric duration distribution)\n"
    "  -p state-level-pruning (HSMM)\n"
    "  -P state-level-pruning (HMM)\n"
    "  -d extra-duration-search-space\n"
    "  -i (isolated alignment)\n"
    "  -h (print usage)\n");
  exit(1);
}

int opt_geodur = 0;
int opt_embdalign = 1;

static cJSON* align(lrh_model* hsmm, lrh_observ* o, cJSON* j_states) {
  FP_TYPE* outp = NULL;
  if(! opt_embdalign) {
      lrh_dataset* d = load_isolated_data_from_json(j_states, o);
      int nsample = d -> observset -> nsample;
      int** realign_all = calloc(nsample, sizeof(int*));
      for(int e = 0; e < nsample; e ++) {
        lrh_seg* es = d -> segset -> samples[e];
        lrh_observ* eo = d -> observset -> samples[e];
        for(int i = 0; i < es -> nseg; i ++)
          if(es -> time[i] > eo -> nt)
            es -> time[i] = eo -> nt;
        lrh_seg_buildjumps(es);
        if(opt_geodur) {
          outp = lrh_sample_outputprob_lg_full(hsmm, eo, es);
          int* realign = lrh_viterbi_geometric(hsmm, es, outp, eo -> nt, NULL);
          realign_all[e] = calloc(es -> nseg * 2 + 2, sizeof(int));
          for(int i = 0; i < es -> nseg; i ++) {
            realign_all[e][i * 2 + 0] = realign[i];
            realign_all[e][i * 2 + 1] = i;
          }
          realign_all[e][es -> nseg * 2] = -1;
          free(realign);
        } else {
          outp = lrh_sample_outputprob_lg(hsmm, eo, es);
          int* realign = lrh_viterbi(hsmm, es, outp, eo -> nt, NULL);
          realign_all[e] = realign;
        }
        free(outp);
      }
      int nseg = 0;
      for(int e = 0; e < nsample; e ++) {
        int i = 0;
        while(realign_all[e][i * 2] != -1) i ++;
        nseg += i;
      }
      int t_base = 0;
      int s_base = 0;
      int* realign = calloc(nseg * 2 + 2, sizeof(int));
      realign[nseg * 2] = -1;
      nseg = 0;
      for(int e = 0; e < nsample; e ++) {
        int i = 0;
        while(realign_all[e][i * 2] != -1) {
          realign[nseg * 2 + 0] = realign_all[e][i * 2 + 0] + t_base;
          realign[nseg * 2 + 1] = realign_all[e][i * 2 + 1] + s_base;
          nseg ++;
          i ++;
        }
        t_base = realign[nseg * 2 - 2];
        s_base += d -> segset -> samples[e] -> nseg;
        free(realign_all[e]);
      }
      free(realign_all);
      lrh_seg* s = load_seg_from_json(j_states, hsmm -> nstream);
      j_states = json_from_seg_shuffle(s, j_states, realign);
      lrh_delete_seg(s);
      free(realign);
      delete_dataset(d);
  } else {
    lrh_seg* s = load_seg_from_json(j_states, hsmm -> nstream);
    for(int i = 0; i < s -> nseg; i ++)
      if(s -> time[i] > o -> nt)
        s -> time[i] = o -> nt;
    lrh_seg_buildjumps(s);

    int* realign = NULL;
    if(opt_geodur) {
      outp = lrh_sample_outputprob_lg_full(hsmm, o, s);
      realign = lrh_viterbi_geometric(hsmm, s, outp, o -> nt, NULL);
      for(int i = 0; i < s -> nseg; i ++)
        s -> time[i] = realign[i];
      j_states = json_from_seg(s, j_states);
    } else {
      outp = lrh_sample_outputprob_lg(hsmm, o, s);
      realign = lrh_viterbi(hsmm, s, outp, o -> nt, NULL);
      j_states = json_from_seg_shuffle(s, j_states, realign);
    }
    free(outp); free(realign);
    lrh_delete_seg(s);
  }
  return j_states;
}

extern char* optarg;
int main(int argc, char** argv) {
# ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
# endif
  int c;
  cJSON* j_segm = NULL;
  lrh_model* hsmm = NULL;

  while((c = getopt(argc, argv, "m:s:gp:P:d:ih")) != -1) {
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
    case 'g':
      opt_geodur = 1;
    break;
    case 'p':
      lrh_inference_stprune = atoi(optarg);
    break;
    case 'P':
      lrh_inference_stprune_full_slope = atof(optarg);
    break;
    case 'd':
      lrh_inference_duration_extra = atoi(optarg);
    break;
    case 'i':
      opt_embdalign = 0;
    break;
    case 'h':
      print_usage();
    break;
    default:
      abort();
    }
  }
  if(j_segm == NULL) {
    fprintf(stderr, "Error: cegmentation file is not specified.\n");
    return 1;
  }
  if(hsmm == NULL) {
    fprintf(stderr, "Error: model file is not specified.\n");
    return 1;
  }

  cJSON* j_file_list = cJSON_GetObjectItem(j_segm, "file_list");
  checkvar(file_list);
  int nfile = cJSON_GetArraySize(j_file_list);

  lrh_model_precompute(hsmm);
  for(int f = 0; f < nfile; f ++) {
    cJSON* j_file_list_f = cJSON_GetArrayItem(j_file_list, f);
    cJSON* j_filename = cJSON_GetObjectItem(j_file_list_f, "filename");
    checkvar(filename);
    cJSON* j_states = cJSON_GetObjectItem(j_file_list_f, "states");
    checkvar(states);

    lrh_observ* o = load_observ_from_float(j_filename -> valuestring, hsmm);
    j_states = align(hsmm, o, j_states);
    cJSON_ReplaceItemInObject(j_file_list_f, "states", j_states);
    lrh_delete_observ(o);
  }

  char* jsonstr = cJSON_Print(j_segm);
  printf("%s\n", jsonstr);
  free(jsonstr);

  cJSON_Delete(j_segm);
  lrh_delete_model(hsmm);
  return 0;
}

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
#include "external/liblrhsmm/serial.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "cli-common.h"

static void print_usage() {
  fprintf(stderr,
    "shiro-untie\n"
    "  -m model-file\n"
    "  -s segmentation-file\n"
    "  -o output-segmentation-file\n"
    "  -O output-summary-file\n"
    "  -h (print usage)\n");
  exit(1);
}

FILE* fp_out_segm = NULL;
FILE* fp_out_summary = NULL;

static int get_total_num_states(cJSON* j_file_list) {
  int nfile = cJSON_GetArraySize(j_file_list);
  int ntotal = 0;
  for(int f = 0; f < nfile; f ++) {
    cJSON* j_file_list_f = cJSON_GetArrayItem(j_file_list, f);
    cJSON* j_filename = cJSON_GetObjectItem(j_file_list_f, "filename");
    checkvar(filename);
    cJSON* j_states = cJSON_GetObjectItem(j_file_list_f, "states");
    checkvar(states);
    ntotal += cJSON_GetArraySize(j_states);
  }
  return ntotal;
}

static void copy_dur(lrh_model* dst, lrh_model* src, int dstidx, int srcidx) {
  dst -> durations[dstidx] = lrh_create_duration();
  *dst -> durations[dstidx] = *src -> durations[srcidx];
  dst -> durations[dstidx] -> _tmp_prep = NULL;
}

static void copy_out(lrh_model* dst, lrh_model* src, int dstidx, int* srcidx) {
  int nstream = src -> nstream;
  for(int l = 0; l < nstream; l ++) {
    dst -> streams[l] -> gmms[dstidx] = lrh_gmm_copy(
      src -> streams[l] -> gmms[srcidx[l]]);
  }
}

extern char* optarg;
int main(int argc, char** argv) {
# ifdef _WIN32
  _setmode(_fileno(stdout), _O_BINARY);
# endif
  int c;
  cJSON* j_segm = NULL;
  lrh_model* hsmm = NULL;

  while((c = getopt(argc, argv, "m:s:o:O:h")) != -1) {
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
    case 'o':
      fp_out_segm = fopen(optarg, "w");
      if(fp_out_segm == NULL) {
        fprintf(stderr, "Error: cannot create %s.\n", optarg);
        return 1;
      }
    break;
    case 'O':
      fp_out_summary = fopen(optarg, "w");
      if(fp_out_summary == NULL) {
        fprintf(stderr, "Error: cannot create %s.\n", optarg);
        return 1;
      }
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
  int nstate = get_total_num_states(j_file_list);
  lrh_model* cdhsmm = lrh_create_empty_model(hsmm -> nstream, nstate);
  for(int l = 0; l < hsmm -> nstream; l ++) {
    cdhsmm -> streams[l] = lrh_create_empty_stream(nstate);
  }
  
  int state = 0;
  for(int f = 0; f < nfile; f ++) {
    cJSON* j_file_list_f = cJSON_GetArrayItem(j_file_list, f);
    cJSON* j_states = cJSON_GetObjectItem(j_file_list_f, "states");
    int nseg = cJSON_GetArraySize(j_states);
    for(int i = 0; i < nseg; i ++) {
      cJSON* j_states_i = cJSON_GetArrayItem(j_states, i);
      cJSON* j_dur = cJSON_GetObjectItem(j_states_i, "dur");
      cJSON* j_out = cJSON_GetObjectItem(j_states_i, "out");
      
      if(fp_out_summary != NULL) {
        fprintf(fp_out_summary, "%d %d %d", state, f, i);
        cJSON* j_ext = cJSON_GetObjectItem(j_states_i, "ext");
        if(j_ext != NULL) {
          cJSON* j_phoneme = j_ext -> child;
          cJSON* j_stateidx = j_phoneme -> next;
          fprintf(fp_out_summary, " %s %d",
            j_phoneme -> valuestring, j_stateidx -> valueint);
        }
      }
      
      int* outst = calloc(cdhsmm -> nstream, sizeof(int));
      for(int l = 0; l < hsmm -> nstream; l ++) {
        cJSON* j_out_l = cJSON_GetArrayItem(j_out, l);
        outst[l] = j_out_l -> valueint;
        cJSON_ReplaceItemInArray(j_out, l, cJSON_CreateNumber(state));
      }
      copy_out(cdhsmm, hsmm, state, outst);
      copy_dur(cdhsmm, hsmm, state, j_dur -> valueint);
      cJSON_ReplaceItemInObject(j_states_i, "dur", cJSON_CreateNumber(state));
      free(outst);
      state ++;
      if(fp_out_summary != NULL) fputs("\n", fp_out_summary);
    }
  }
  
  cmp_ctx_t cmpobj;
  cmp_init(& cmpobj, stdout, file_reader, file_writer);
  lrh_write_model(& cmpobj, cdhsmm);
  
  if(fp_out_segm != NULL) {
    char* jsonstr = cJSON_Print(j_segm);
    fprintf(fp_out_segm, "%s\n", jsonstr);
    free(jsonstr);
    fclose(fp_out_segm);
  }
  if(fp_out_summary != NULL)
    fclose(fp_out_summary);
  
  cJSON_Delete(j_segm);
  lrh_delete_model(hsmm);
  lrh_delete_model(cdhsmm);
  return 0;
}


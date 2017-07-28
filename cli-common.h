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

#include <string.h>

inline static bool read_bytes(void* data, size_t sz, FILE* fh) {
  return fread(data, sizeof(uint8_t), sz, fh) == (sz * sizeof(uint8_t));
}

inline static bool file_reader(cmp_ctx_t* ctx, void* data, size_t limit) {
  return read_bytes(data, limit, (FILE*)ctx->buf);
}

inline static size_t file_writer(cmp_ctx_t* ctx, const void* data, size_t count) {
  return fwrite(data, sizeof(uint8_t), count, (FILE*)ctx->buf);
}

static char* readall(const char* path) {
  FILE* fp = fopen(path, "r");
  if(fp == NULL) return NULL;
  fseek(fp, 0, SEEK_END);
  int fsize = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  char* ret = malloc(fsize + 1);
  ret[fsize] = 0;
  fread(ret, 1, fsize, fp);
  fclose(fp);
  return ret;
}

static lrh_model* load_model(const char* path) {
  cmp_ctx_t cmpobj;
  lrh_model* h = NULL;
  if(! strcmp(path, "-")) {
    cmp_init(& cmpobj, stdin, file_reader, file_writer);
    h = lrh_read_model(& cmpobj);
  } else {
    FILE* fin = fopen(path, "rb");
    if(fin == NULL) return NULL;
    cmp_init(& cmpobj, fin, file_reader, file_writer);
    h = lrh_read_model(& cmpobj);
    fclose(fin);
  }
  return h;
}

static lrh_observ* load_observ_from_float(const char* path, lrh_model* h) {
  FILE* fin = fopen(path, "rb");
  if(fin == NULL) return NULL;
  
  int ndim[64];
  int stride = 0;
  for(int l = 0; l < h -> nstream; l ++) {
    ndim[l] = h -> streams[l] -> gmms[0] -> ndim;
    stride += h -> streams[l] -> gmms[0] -> ndim;
  }
  fseek(fin, 0, SEEK_END);
  int fsize = ftell(fin);
  fseek(fin, 0, SEEK_SET);

  if(fsize % stride * 4 != 0) {
    fprintf(stderr, "Error: file size of %s does not match with the model.\n", path);
    fclose(fin);
    return NULL;
  }
  
  int nt = fsize / stride / 4;
  float* fdata = malloc(fsize);
  fread(fdata, 4, fsize / 4, fin);
  fclose(fin);

  lrh_observ* o = lrh_create_observ(h -> nstream, nt, ndim);
  int c = 0;
  for(int t = 0; t < nt; t ++)
    for(int l = 0; l < h -> nstream; l ++)
      for(int i = 0; i < ndim[l]; i ++) {
        lrh_obm(o, t, i, l) = fdata[c ++];
      }

  free(fdata);
  return o;
}

#define checkvar(name) \
  if(j_##name == NULL) { \
    fprintf(stderr, "Error: missing JSON attribute: \"%s\"\n", #name); \
    exit(1); \
  }

static lrh_seg* load_seg_from_json(cJSON* j_states, int nstream) {
  int nseg = cJSON_GetArraySize(j_states);
  lrh_seg* s = lrh_create_seg(nstream, nseg);
  for(int i = 0; i < nseg; i ++) {
    cJSON* j_states_i = cJSON_GetArrayItem(j_states, i);
    cJSON* j_time = cJSON_GetObjectItem(j_states_i, "time");
    cJSON* j_dur  = cJSON_GetObjectItem(j_states_i, "dur");
    checkvar(dur);
    cJSON* j_out  = cJSON_GetObjectItem(j_states_i, "out");
    checkvar(out);
    s -> durstate[i] = j_dur -> valueint;
    if(cJSON_GetArraySize(j_out) != nstream) {
      fprintf(stderr, "Error: number of output streams does not match.\n");
      exit(1);
    }
    for(int l = 0; l < nstream; l ++) {
      cJSON* j_out_l = cJSON_GetArrayItem(j_out, l);
      s -> outstate[l][i] = j_out_l -> valueint;
    }
    if(j_time != NULL) {
      s -> time[i] = j_time -> valueint;
    }
    cJSON* j_jmp = cJSON_GetObjectItem(j_states_i, "jmp");
    if(j_jmp != NULL) {
      int njmp = cJSON_GetArraySize(j_jmp);
      FP_TYPE pnext = 1.0;
      s -> djump_out[i] = realloc(s -> djump_out[i], (njmp + 1) * sizeof(int));
      s -> pjump_out[i] = realloc(s -> pjump_out[i], (njmp + 1) * sizeof(FP_TYPE));
      for(int k = 0; k < njmp; k ++) {
        cJSON* j_jmp_k = cJSON_GetArrayItem(j_jmp, k);
        cJSON* j_jmp_d = cJSON_GetObjectItem(j_jmp_k, "d");
        cJSON* j_jmp_p = cJSON_GetObjectItem(j_jmp_k, "p");
        checkvar(jmp_d); checkvar(jmp_p);
        if(j_jmp_d ->valueint != 1) {
          s -> djump_out[i][k] = j_jmp_d -> valueint;
          s -> pjump_out[i][k] = j_jmp_p -> valuedouble;
          pnext -= j_jmp_p -> valuedouble;
        }
      }
      s -> djump_out[i][njmp] = 1;
      s -> pjump_out[i][njmp] = pnext;
    }
  }
  return s;
}

// if j_states_in is available (i.e. not NULL), copy over ext attribute
static cJSON* json_from_seg(lrh_seg* s, cJSON* j_states_in) {
  cJSON* j_states = cJSON_CreateArray();
  cJSON* j_curr_in = j_states_in == NULL ? NULL : j_states_in -> child;
  for(int i = 0; i < s -> nseg; i ++) {
    cJSON* j_states_i = cJSON_CreateObject();
    cJSON_AddNumberToObject(j_states_i, "time", s -> time[i]);
    cJSON_AddNumberToObject(j_states_i, "dur", s -> durstate[i]);
    cJSON* j_out = cJSON_CreateArray();
    for(int l = 0; l < s -> nstream; l ++)
      cJSON_AddItemToArray(j_out, cJSON_CreateNumber(s -> outstate[l][i]));
    cJSON_AddItemToObject(j_states_i, "out", j_out);
    if(j_states_in != NULL && j_curr_in != NULL) {
      cJSON* j_ext = cJSON_GetObjectItem(j_curr_in, "ext");
      if(j_ext != NULL)
        cJSON_AddItemToObject(j_states_i, "ext", cJSON_Duplicate(j_ext, 1));
      cJSON* j_jmp = cJSON_GetObjectItem(j_curr_in, "jmp");
      if(j_jmp != NULL)
        cJSON_AddItemToObject(j_states_i, "jmp", cJSON_Duplicate(j_jmp, 1));
      j_curr_in = j_curr_in -> next;
    }
    cJSON_AddItemToArray(j_states, j_states_i);
  }
  return j_states;
}

static cJSON* json_from_seg_shuffle(lrh_seg* s, cJSON* j_states_in, int* shufidx) {
  int nreseg = 0;
  while(shufidx[nreseg * 2] != -1) nreseg ++;

  cJSON* j_states = cJSON_CreateArray();

  for(int i = 0; i < nreseg; i ++) {
    int it = shufidx[i * 2 + 0];
    int is = shufidx[i * 2 + 1];
    cJSON* j_states_i = cJSON_CreateObject();
    cJSON_AddNumberToObject(j_states_i, "time", it);
    cJSON_AddNumberToObject(j_states_i, "dur", s -> durstate[is]);
    cJSON* j_out = cJSON_CreateArray();
    for(int l = 0; l < s -> nstream; l ++)
      cJSON_AddItemToArray(j_out, cJSON_CreateNumber(s -> outstate[l][is]));
    cJSON_AddItemToObject(j_states_i, "out", j_out);
    if(j_states_in != NULL) {
      cJSON* j_curr_in = cJSON_GetArrayItem(j_states_in, is);
      cJSON* j_ext = cJSON_GetObjectItem(j_curr_in, "ext");
      if(j_ext != NULL)
        cJSON_AddItemToObject(j_states_i, "ext", cJSON_Duplicate(j_ext, 1));
    }
    cJSON_AddItemToArray(j_states, j_states_i);
  }
  return j_states;
}

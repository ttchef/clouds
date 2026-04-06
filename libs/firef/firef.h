#ifndef FIREF_H
#define FIREF_H 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float *vertices;
    size_t vertex_count;

    unsigned int *indices;
    size_t index_count;
} Obj;

static inline float parse_float(const char *s) {
    return strtof(s, NULL);
}

static inline int parse_int(const char *s) {
    return strtol(s, NULL, 10);
}

#ifdef __cplusplus
}
#endif 

#endif

#ifdef FIREF_IMPL

Obj load_obj(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "Failed to open %s\n", path);
        exit(1);
    }

    float *positions = NULL, *uvs = NULL, *normals = NULL;
    size_t pos_len = 0, uv_len = 0, norm_len = 0;
    float *vertices = NULL;
    size_t vert_len = 0, vert_cap = 0;
    unsigned int *indices = NULL;
    size_t idx_len = 0, idx_cap = 0;

    char line[512];
    unsigned int vertex_counter = 0;

    while (fgets(line, sizeof(line), file)) {
        char* p = line;  
        char* end_ptr = NULL;
        float x = 0, y = 0, z = 0; 

        while (isspace(*p) && *p != '\n' && *p != '\0') p++;

        if (strncmp(p, "v", 1) == 0 && isspace(p[1])) { 
            p += 1; 
            while (isspace(*p)) p++; 
            x = strtof(p, &end_ptr); p = end_ptr;
            while (isspace(*p)) p++; 
            y = strtof(p, &end_ptr); p = end_ptr;
            while (isspace(*p)) p++;
            z = strtof(p, &end_ptr); 

            float *tmp = (float*)realloc(positions, (pos_len + 3) * sizeof(float));
            if (!tmp) exit(1);
            positions = tmp;
            positions[pos_len++] = x;
            positions[pos_len++] = y;
            positions[pos_len++] = z;
        } else if (strncmp(p, "vt", 2) == 0 && isspace(p[2])) { 
            p += 2; 
            while (isspace(*p)) p++;
            x = strtof(p, &end_ptr); p = end_ptr;
            while (isspace(*p)) p++;
            y = strtof(p, &end_ptr);

            float *tmp = (float*)realloc(uvs, (uv_len + 2) * sizeof(float));
            if (!tmp) exit(1);
            uvs = tmp;
            uvs[uv_len++] = x;
            uvs[uv_len++] = y;
        } else if (strncmp(p, "vn", 2) == 0 && isspace(p[2])) { 
            p += 2; 
            while (isspace(*p)) p++;
            x = strtof(p, &end_ptr); p = end_ptr;
            while (isspace(*p)) p++;
            y = strtof(p, &end_ptr); p = end_ptr;
            while (isspace(*p)) p++;
            z = strtof(p, &end_ptr);

            float *tmp = (float*)realloc(normals, (norm_len + 3) * sizeof(float));
            if (!tmp) exit(1);
            normals = tmp;
            normals[norm_len++] = x;
            normals[norm_len++] = y;
            normals[norm_len++] = z;
        } else if (strncmp(p, "f", 1) == 0 && isspace(p[1])) {
            char *face_data_start = p + 1; 
            while(isspace(*face_data_start)) face_data_start++;

            char *token = strtok(face_data_start, " \n"); 
            unsigned int face[32];
            int count = 0;

            while (token && count < 32) {
                int current_vi = -1;
                int current_ti = -1;
                int current_ni = -1;

                char* p_token = token;
                char* end_ptr_face; 

                current_vi = strtol(p_token, &end_ptr_face, 10);
                if (p_token == end_ptr_face) { 
                    fprintf(stderr, "Error parsing vertex index in face line: %s\n", token);
                    exit(1);
                }
                p_token = end_ptr_face; 

                
                if (*p_token == '/') {
                    p_token++; 
                    
                    if (*p_token != '/') { 
                        current_ti = strtol(p_token, &end_ptr_face, 10);
                        p_token = end_ptr_face; 
                    }
                    
                    
                    if (*p_token == '/') { 
                        p_token++; 
                        current_ni = strtol(p_token, &end_ptr_face, 10);
                    }
                }
                
                if (current_vi > 0) current_vi--; 
                if (current_ti > 0) current_ti--; 
                if (current_ni > 0) current_ni--; 

                float vx = positions[current_vi * 3 + 0];
                float vy = positions[current_vi * 3 + 1];
                float vz = positions[current_vi * 3 + 2];

                float tx = (current_ti >= 0 && (size_t)current_ti * 2 + 1 < uv_len) ? uvs[current_ti * 2 + 0] : 0.0f;
                float ty = (current_ti >= 0 && (size_t)current_ti * 2 + 1 < uv_len) ? uvs[current_ti * 2 + 1] : 0.0f;

                
                float nx = (current_ni >= 0 && (size_t)current_ni * 3 + 2 < norm_len) ? normals[current_ni * 3 + 0] : 0.0f;
                float ny = (current_ni >= 0 && (size_t)current_ni * 3 + 2 < norm_len) ? normals[current_ni * 3 + 1] : 0.0f; 
                float nz = (current_ni >= 0 && (size_t)current_ni * 3 + 2 < norm_len) ? normals[current_ni * 3 + 2] : 0.0f; 

                
                if (vert_len + 8 > vert_cap) {
                    size_t new_cap = vert_cap == 0 ? 64 : vert_cap * 2;
                    float *tmp = (float*)realloc(vertices, new_cap * sizeof(float));
                    if (!tmp) exit(1);
                    vertices = tmp;
                    vert_cap = new_cap;
                }

                vertices[vert_len++] = vx;
                vertices[vert_len++] = vy;
                vertices[vert_len++] = vz;
                vertices[vert_len++] = tx;
                vertices[vert_len++] = ty;
                vertices[vert_len++] = nx;
                vertices[vert_len++] = ny;
                vertices[vert_len++] = nz;

                face[count++] = vertex_counter++;

                
                token = strtok(NULL, " \n");
            }

            for (int i = 1; i < count - 1; ++i) {
                if (idx_len + 3 > idx_cap) {
                    size_t new_cap = idx_cap == 0 ? 64 : idx_cap * 2;
                    unsigned int *tmp = (unsigned int*)realloc(indices, new_cap * sizeof(unsigned int));
                    if (!tmp) exit(1);
                    indices = tmp;
                    idx_cap = new_cap;
                }

                indices[idx_len++] = face[0];
                indices[idx_len++] = face[i];
                indices[idx_len++] = face[i + 1];
            }
        }    
    }

    fclose(file);
    free(positions);
    free(uvs);
    free(normals);

    Obj obj = {
        .vertices = vertices,
        .vertex_count = vert_len,
        .indices = indices,
        .index_count = idx_len
    };
    return obj;
}

void free_obj(Obj *obj) {
    free(obj->vertices);
    free(obj->indices);
}

#endif

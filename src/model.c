
#include <darray.h>
#include <log.h>
#include <model.h>
#include <renderer.h>
#include <texture.h>

#include <cgltf/cgltf.h>
#include <stbi/stb_image.h>

#include <assert.h>

static void fill_buffer(u32 input_stride, void *input_data, u32 output_stride,
                        void *output_data, u32 n_elements, u32 element_size) {
    u8 *output = output_data;
    u8 *input = input_data;

    for (u32 i = 0; i < n_elements; i++) {
        for (u32 j = 0; j < element_size; j++) {
            output[j] = input[j];
        }
        output += output_stride;
        input += input_stride;
    }
}

model_id model_create_file(struct renderer *r, const char *path) {
    struct model model = {0};
    model.valid = true;
    model.texture = NO_TEXTURE;

    cgltf_options options = {0};
    cgltf_data *data = NULL;

    cgltf_result error = cgltf_parse_file(&options, path, &data);
    if (error != cgltf_result_success) {
        LOGM(ERROR, "failed to load gltf model: %s", path);
        return NO_MODEL;
    }

    // TODO: path
    error = cgltf_load_buffers(&options, data, "assets/models");
    if (error != cgltf_result_success) {
        LOGM(ERROR, "failed to load gltf model bufffers: %s", path);
        cgltf_free(data);
        return NO_MODEL;
    }

    // check for unsupported model (that my program cant handle rn xD)
    cgltf_primitive *p = &data->meshes[0].primitives[0];
    assert(data->meshes_count == 1);
    assert(data->meshes[0].primitives_count == 1);
    assert(p->attributes_count > 0);
    assert(p->indices->component_type == cgltf_component_type_r_16u);
    assert(p->indices->stride == sizeof(u16));

    // index buffer
    u8 *buffer_base = (u8 *)p->indices->buffer_view->buffer->data;
    u64 index_data_size = p->indices->buffer_view->size;
    void *index_data = buffer_base + p->indices->buffer_view->offset;

    model.index_buffer = vk_buffer_create_device_local(
        &r->init, index_data_size,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    vk_buffer_upload_data(&r->init, &model.index_buffer, index_data_size,
                          index_data);
    model.n_index = p->indices->count;

    // vertex buffer
    // pos (3) uv (2) normals (3)
    u32 output_stride = sizeof(f32) * 8;
    u32 n_vertex = p->attributes->data->count;
    u32 vertex_data_size = output_stride * n_vertex;
    u8 *vertex_data = malloc(vertex_data_size);

    for (u32 i = 0; i < p->attributes_count; i++) {
        cgltf_attribute *a = p->attributes + i;
        buffer_base = (u8 *)a->data->buffer_view->buffer->data;
        u32 input_stride = a->data->stride;

        if (a->type == cgltf_attribute_type_position) {
            void *pos_data = buffer_base + a->data->buffer_view->offset;
            fill_buffer(input_stride, pos_data, output_stride, vertex_data,
                        n_vertex, sizeof(f32) * 3);
        }

        else if (a->type == cgltf_attribute_type_texcoord) {
            void *texcoord_data = buffer_base + a->data->buffer_view->offset;
            fill_buffer(input_stride, texcoord_data, output_stride,
                        vertex_data + (sizeof(f32) * 3), n_vertex,
                        sizeof(f32) * 2);
        }

        else if (a->type == cgltf_attribute_type_normal) {
            void *normal_data = buffer_base + a->data->buffer_view->offset;
            fill_buffer(input_stride, normal_data, output_stride,
                        vertex_data + (sizeof(f32) * 5), n_vertex,
                        sizeof(f32) * 3);
        }
    }

    model.vertex_buffer = vk_buffer_create_device_local(
        &r->init, vertex_data_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    vk_buffer_upload_data(&r->init, &model.vertex_buffer, vertex_data_size,
                          vertex_data);

    // texture
    if (data->materials_count != 1) {
        LOGM(WARN, "model has no texture: %s", path);
        goto end;
    }

    cgltf_material *material = &data->materials[0];
    if (!material->has_pbr_metallic_roughness) {
        LOGM(WARN, "model has no texture: %s", path);
        goto end;
    }

    cgltf_texture_view albedo_texture_view =
        material->pbr_metallic_roughness.base_color_texture;

    if (albedo_texture_view.has_transform ||
        albedo_texture_view.texcoord != 0 || !albedo_texture_view.texture) {
        LOGM(WARN, "model has no texture: %s", path);
        goto end;
    }

    cgltf_texture *albedo_texture = albedo_texture_view.texture;

    cgltf_buffer_view *buffer_view = albedo_texture->image->buffer_view;

    if (buffer_view->size >= INT32_MAX) {
        LOGM(WARN, "model has no texture: %s", path);
        goto end;
    }

    u8 *data_ptr = (u8 *)buffer_view->buffer->data + buffer_view->offset;

    i32 bpp, width, height;
    u8 *texture_data = stbi_load_from_memory(
        (stbi_uc *)data_ptr, buffer_view->size, &width, &height, &bpp, 4);
    if (!texture_data) {
        LOGM(ERROR, "failed to load model texture: %s", path);
        return NO_MODEL;
    }

    bpp = 4;

    model.texture = texture_create(r, width, height, texture_data);
    if (model.texture == NO_TEXTURE) {
        LOGM(ERROR, "failed to create model texture: %s", path);
    }

    stbi_image_free(texture_data);

end:

    cgltf_free(data);

    model_id id = darrayLength(r->models);
    darrayPush(r->models, model);

    return id;
}

void model_destroy(struct renderer *r, model_id id) {
    if (id >= (i32)darrayLength(r->models)) {
        LOGM(ERROR, "inavlid model id");
        return;
    }

    struct model *model = &r->models[id];

    vmaDestroyBuffer(r->init.allocator, model->vertex_buffer.handle,
                     model->vertex_buffer.alloc);
    vmaDestroyBuffer(r->init.allocator, model->index_buffer.handle,
                     model->index_buffer.alloc);
    *model = (struct model){0};
}

bool model_set_texture(struct renderer *r, model_id model, texture_id texture) {
    if (model >= (i32)darrayLength(r->models)) {
        LOGM(ERROR, "invalid model index: %d", model);
        return false;
    }

    if (texture > MAX_TEXTURES || texture < 0) {
        LOGM(ERROR, "inavlid texture index: %d", texture);
        return false;
    }

    struct model *m = &r->models[model];
    if (m->texture != NO_TEXTURE) {
        LOGM(API_DUMP, "overwriting model texture");
    }

    m->texture = texture;

    return true;
}

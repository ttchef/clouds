
#extension GL_GOOGLE_include_directive : require

#include "../../shader_shared.h"

struct DirLight {
    vec4 direction;
    vec4 color;  
};

struct PointLight {
    vec4 pos;
    vec4 color;

    float constant;
    float linear;
    float quadratic;

    float padding;  
};

struct SpotLight {  
    vec4 pos;
    vec4 color;

    float cutt_off;
    float outer_cutt_off;

    float constant;
    float linear;
    float quadratic;

    vec3 padding;  
};

vec3 calc_dir_light(DirLight light, vec3 normal, vec3 view_dir, vec3 tex_sample) {
    vec3 light_dir = normalize(-light.direction.xyz);

    float diff = max(dot(normal, light_dir), 0.0);

    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

    vec3 ambient =  0.15 * light.color.xyz * tex_sample;
    vec3 diffiuse = light.color.xyz * diff * tex_sample;
    vec3 specular = vec3(1.0) * spec * tex_sample;

    return (ambient + diffiuse + specular);
}

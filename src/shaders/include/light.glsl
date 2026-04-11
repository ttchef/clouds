
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

vec3 calc_dir_light(DirLight light, vec3 normal, vec3 view_dir, vec3 surface_color) {
    vec3 light_dir = normalize(-light.direction.xyz);

    float diff = max(dot(normal, light_dir), 0.0);

    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

    vec3 ambient =  0.15 * light.color.xyz * surface_color;
    vec3 diffiuse = light.color.xyz * diff * surface_color;
    vec3 specular = vec3(1.0) * spec * surface_color;

    return (ambient + diffiuse + specular);
}

vec3 calc_point_light(PointLight light, vec3 normal, vec3 frag_pos, vec3 view_dir, vec3 surface_color) {
    vec3 light_dir = normalize(light.pos.xyz - frag_pos);
    float diff = max(dot(normal, light_dir), 0.0);

    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);
    
    float distance = length(light.pos.xyz - frag_pos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    vec3 ambient = 0.15 * light.color.xyz * surface_color * attenuation;
    vec3 diffiuse = light.color.xyz * diff * surface_color * attenuation;
    vec3 specular = light.color.xyz * spec * surface_color * attenuation;

    return (ambient + diffiuse + specular);
}

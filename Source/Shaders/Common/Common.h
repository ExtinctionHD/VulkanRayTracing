#ifndef COMMON_H
#define COMMON_H

#ifdef __cplusplus
#define mat4 glm::mat4
#define vec4 glm::vec4
#endif

struct LightingData
{
    vec4 direction;
    vec4 colorIntensity;
};

#ifdef __cplusplus
#undef mat4
#undef vec4
#endif

#endif

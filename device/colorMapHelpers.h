#pragma once

#include <vector>

namespace anari_cycles {

    // Ramp point only stores color (position is implicit, evenly spaced)
    struct ColorRampPoint {
        float4 rgba;  // (r,g,b,a)
    };

    inline void colorramp_to_array(const std::vector<ColorRampPoint>& color_ramp,
        ccl::array<float3>& ramp_values,
        ccl::array<float>& ramp_alpha,
        int table_size)
    {
        ramp_values.resize(table_size);
        ramp_alpha.resize(table_size);

        if (color_ramp.empty()) {
            for (int i = 0; i < table_size; i++) {
                ramp_values[i] = make_float3(0.0f, 0.0f, 0.0f);
                ramp_alpha[i] = 1.0f;
            }
            return;
        }

        for (int i = 0; i < table_size; i++) {
            float t = (float)i / (table_size - 1);

            // Map t into ramp indices
            float fidx = t * (color_ramp.size() - 1);
            int idx0 = (int)fidx;
            int idx1 = std::min(idx0 + 1, (int)color_ramp.size() - 1);
            float fac = fidx - idx0;

            const float4& c0 = color_ramp[idx0].rgba;
            const float4& c1 = color_ramp[idx1].rgba;

            // Linear interpolation
            float4 c;
            c.x = c0.x * (1.0f - fac) + c1.x * fac;
            c.y = c0.y * (1.0f - fac) + c1.y * fac;
            c.z = c0.z * (1.0f - fac) + c1.z * fac;
            c.w = c0.w * (1.0f - fac) + c1.w * fac;

            ramp_values[i] = make_float3(c.x, c.y, c.z);
            ramp_alpha[i] = c.w;
        }
    };

} // namespace anari_cycles

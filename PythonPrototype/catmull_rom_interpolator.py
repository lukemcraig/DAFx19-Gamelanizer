"""
This is a translation from CatmullRomInterpolator class in the JUCE C++ library. It's just to prototype with.
"""
import numpy as np


class CatmullRomInterpolator:
    def __init__(self):
        self.last_input_samples = np.zeros(5)
        self.sub_sample_pos = 1.0
        pass

    def reset(self):
        self.sub_sample_pos = 1.0
        self.last_input_samples[:] = 0.0
        pass

    def process(self, actual_ratio, input_buffer, output_buffer, num_out):
        num_used, self.sub_sample_pos = interpolate(self.last_input_samples, self.sub_sample_pos, actual_ratio,
                                                    input_buffer, output_buffer, num_out)
        return num_used


def push_interpolation_sample(last_input_samples, new_value):
    last_input_samples[4] = last_input_samples[3]
    last_input_samples[3] = last_input_samples[2]
    last_input_samples[2] = last_input_samples[1]
    last_input_samples[1] = last_input_samples[0]
    last_input_samples[0] = new_value
    return


def push_interpolation_samples(last_input_samples, input_buffer, num_out):
    if num_out >= 5:
        for i in range(5):
            num_out -= 1
            last_input_samples[i] = input_buffer[num_out]
    else:
        for i in range(num_out):
            push_interpolation_sample(last_input_samples, input_buffer[i])
    return


def interpolate(last_input_samples, sub_sample_pos, actual_ratio, input_buffer, output_buffer, num_out):
    pos = sub_sample_pos
    if actual_ratio == 1 and pos == 1:
        output_buffer[:num_out] = input_buffer[:num_out]
        push_interpolation_samples(last_input_samples, input_buffer, num_out)
        return num_out, sub_sample_pos

    num_used = 0

    while num_out > 0:
        while pos >= 1.0:
            push_interpolation_sample(last_input_samples, input_buffer[num_used])
            num_used += 1
            pos -= 1
        output_buffer[0] = value_at_offset_cr(last_input_samples, pos)
        output_buffer = output_buffer[1:]
        pos += actual_ratio
        num_out -= 1

    sub_sample_pos = pos
    return num_used, sub_sample_pos


def value_at_offset_cr(inputs, offset):
    y0 = inputs[3]
    y1 = inputs[2]
    y2 = inputs[1]
    y3 = inputs[0]
    half_y0 = 0.5 * y0
    half_y3 = 0.5 * y3

    return y1 + offset * ((0.5 * y2 - half_y0)
                          + (offset * (((y0 + 2.0 * y2) - (half_y3 + 2.5 * y1))
                                       + (offset * ((half_y3 + 1.5 * y1) - (half_y0 + 1.5 * y2))))))

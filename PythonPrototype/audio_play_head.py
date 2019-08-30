class AudioPlayHead:
    def __init__(self, tempo_markers):
        self.tempo_markers = tempo_markers
        self.time_in_seconds = 0.0
        self.time_in_samples = 0

    def move_to_sample(self, sample, sample_rate):
        self.time_in_seconds = sample / sample_rate
        self.time_in_samples = sample
        return

    def get_current_position(self):
        i, ppqPosition = self.__find_current_tempo_marker_and_ppqs_before_it()

        current_marker = self.tempo_markers[i]
        duration_s = self.time_in_seconds - current_marker.time_position
        ppqPosition += (current_marker.bpm / 60) * duration_s

        return current_marker.bpm, ppqPosition, self.time_in_samples, self.time_in_seconds

    def __find_current_tempo_marker_and_ppqs_before_it(self):
        i = 0
        ppqPosition = 0
        for j in range(1, len(self.tempo_markers)):
            current_marker = self.tempo_markers[j]
            if current_marker.time_position > self.time_in_seconds:
                break
            prev_marker = self.tempo_markers[j - 1]
            duration_s = current_marker.time_position - prev_marker.time_position
            ppqPosition += (prev_marker.bpm / 60) * duration_s
            i = j
        return i, ppqPosition

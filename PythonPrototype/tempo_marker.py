class TempoMarker:
    def __init__(self, time_position: float, bpm: float, time_sig_top: int, time_sig_bottom: int):
        # in seconds
        self.time_position = time_position
        self.bpm = bpm
        self.time_sig_top = time_sig_top
        self.time_sig_bottom = time_sig_bottom

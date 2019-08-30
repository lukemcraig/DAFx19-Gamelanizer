import argparse

from gamelanizer_offline import main

parser = argparse.ArgumentParser(description='Gamelanize an input audio file. Output is stored in /output_offline/')
parser.add_argument('filename', type=str, help='the filename, in /input/, with extension')
parser.add_argument('-b', '--bpm', type=int, nargs='?', default=90)
parser.add_argument('-w', '--window_size', type=int, nargs='?', default=4096)
parser.add_argument('-d', '--overlap_factor', type=int, nargs='?', default=4)
parser.add_argument('-s', '--num_subdivision_levels', type=int, nargs='?', default=2)
parser.add_argument('-c', '--num_output_channels', type=int, nargs='?', default=2)
parser.add_argument('-m', '--input_mix_balance', type=int, nargs='?', default=0.5)
parser.add_argument('-l', '--subdivision_levels_mix_balance', type=float, nargs='?', default=0.5)

args = parser.parse_args()

main(input_filename=args.filename, bpm=args.bpm, window_size=args.window_size,
     hop_size_denominator=args.overlap_factor,
     num_subdivision_levels=args.num_subdivision_levels,
     num_output_channels=args.num_output_channels, input_mix_balance=args.input_mix_balance,
     subdivision_levels_mix_balance=args.subdivision_levels_mix_balance)

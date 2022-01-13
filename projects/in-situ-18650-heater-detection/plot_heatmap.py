#!/usr/bin/env python3

import numpy as np
import matplotlib
import matplotlib.pyplot as plt
from matplotlib.animation import FFMpegWriter
import csv
import sys
import statistics
import time

# importing movie py libraries
from moviepy.editor import VideoClip
from moviepy.video.io.bindings import mplfig_to_npimage

metadata = dict(title='Movie Test', artist='Matplotlib',
                comment='Movie support!')
writer = FFMpegWriter(fps=15, metadata=metadata)

fig, ax = plt.subplots()

with writer.saving(fig, "writer_test.mp4", 100):

#def make_frame(t):

    nrows = 0
    ts_start = time.time()

    with open(sys.argv[1], 'r') as csvfile:
        logreader = csv.reader(csvfile, delimiter=',', quotechar='"')
        header_row = next(logreader)
        print("HEADER ROWR")
        print(header_row)

        # Get baseline ambient conditions from 
        ambient_row = next(logreader)
        ambient_temp = statistics.mean([ float(x) for x in ambient_row[4:9]])
        ambient_temp = 0

        for row in logreader:
            (U1,U2,U3,U4,U5) = row[4:9]
            print(row)
            print(U1, U2, U3, U4, U5)
            print(f'stats {nrows} rows {time.time() - ts_start} elapsed {nrows / (time.time() - ts_start)} rows/s')
            try:
                U1 = float(U1)
                U2 = float(U2)
                U3 = float(U3)
                U4 = float(U4)
                U5 = float(U5)
            except:
                print("Non-float data, skipping")
                continue

            heatmap = ambient_temp * np.ones((11,9))
            heatmap[2][2] = U4
            heatmap[2][6] = U2
            heatmap[5][4] = U3
            heatmap[8][2] = U1
            heatmap[8][6] = U5

            ax.clear()
            ax.set_title(f'ts {row[0]}')
            ax.annotate('H', (5, 7), color='white')
            ax.imshow(heatmap, interpolation='bicubic')
            writer.grab_frame()
            nrows += 1
            #yield mplfig_to_npimage(fig)

#duration = 2
#animation = VideoClip(make_frame, duration = duration)
#animation.write_videofile('output.webm', fps=25)

# fig, ax = plt.subplots()
# im = ax.imshow(heatmap, interpolation='bicubic')

# Show all ticks and label them with the respective list entries
#ax.set_xticks(np.arange(len(farmers)), labels=farmers)
#x.set_yticks(np.arange(len(vegetables)), labels=vegetables)

# Rotate the tick labels and set their alignment.
#plt.setp(ax.get_xticklabels(), rotation=45, ha="right",
#         rotation_mode="anchor")

# Loop over data dimensions and create text annotations.
#for i in range(len(vegetables)):
#    for j in range(len(farmers)):
#        text = ax.text(j, i, harvest[i, j],
#                       ha="center", va="center", color="w")

#
# fig.tight_layout()
# plt.show()

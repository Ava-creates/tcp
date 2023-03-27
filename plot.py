import numpy as np
import matplotlib.pyplot as plt
import sys
from argparse import ArgumentParser
from matplotlib.pyplot import figure


def scale(a):
    return a/1000000.0

parser = ArgumentParser(description="plot")

parser.add_argument('--dir', '-d',
                    help="Directory to store outputs",
                    required=True)

parser.add_argument('--name', '-n',
                    help="name of the experiment",
                    required=True)

parser.add_argument('--trace', '-tr',
                    help="name of the trace",
                    required=True)

parser.add_argument('--cwnd', '-cwnd',
                    help="cwnd file",
                    required=True)

args = parser.parse_args()

fig = plt.figure(figsize=(21,3), facecolor='w')
ax = plt.gca()


# # plotting the trace file
# f1 = open (args.trace,"r")
# BW = []
# nextTime = 1000
# cnt = 0
# for line in f1:
#     if int(line.strip()) > nextTime:
#         BW.append(cnt*1492*8)
#         cnt = 0
#         nextTime+=1000
#     else:
#         cnt+=1
# f1.close()

# # ax.fill_between(range(len(BW)), 0, list(map(scale,BW)),color='#D3D3D3')

# # plotting throughput
# throughputDL = []
# timeDL = []

# traceDL = open (args.dir+"/"+str(args.name), 'r')
# traceDL.readline()

# tmp = traceDL.readline().strip().split(",")
# bytes = int(tmp[1])
# startTime = float(tmp[0])
# stime=float(startTime)

# for time in traceDL:
#     if (float(time.strip().split(",")[0]) - float(startTime)) <= 1.0:
#         bytes += int(time.strip().split(",")[1])
#     else:
#         throughputDL.append(bytes*8/1000000.0)
#         timeDL.append(float(startTime)-stime)
#         bytes = int(time.strip().split(",")[1])
#         startTime += 1.0

# print (timeDL)
# print (throughputDL)

# plt.plot(timeDL, throughputDL, lw=2, color='r')
# # plt.plot(x,y)
# # plt.ylabel("Throughput (Mbps)")
# # plt.xlabel("Time (s)")

# fig, ax = plt.subplots(1,1, facecolor=(1, 1, 1))
# twin_stacked = ax.twiny().twinx()
# twin1 = twin_stacked.plot(timeDL, throughputDL, color = 'g')
# twin2 = twin_stacked.plot(x, y, color = 'r')

# # plt.xlim([0,300])
# plt.grid(True, which="both")
# plt.savefig(args.dir+'/throughput.pdf',dpi=1000,bbox_inches='tight')

f1 = open (args.trace,"r")
BW = []
nextTime = 1000
cnt = 0
for line in f1:
    if int(line.strip()) > nextTime:
        BW.append(cnt*1492*8)
        cnt = 0
        nextTime+=1000
    else:
        cnt+=1
f1.close()

# ax.fill_between(range(len(BW)), 0, list(map(scale,BW)),color='#D3D3D3')

# plotting throughput
throughputDL = []
timeDL = []

traceDL = open (args.dir+"/"+str(args.name), 'r')
traceDL.readline()

tmp = traceDL.readline().strip().split(",")
bytes = int(tmp[1])
startTime = float(tmp[0])
stime=float(startTime)

for time in traceDL:
    if (float(time.strip().split(",")[0]) - float(startTime)) <= 1.0:
        bytes += int(time.strip().split(",")[1])
    else:
        throughputDL.append(bytes*8/1000000.0)
        timeDL.append(float(startTime)-stime)
        bytes = int(time.strip().split(",")[1])
        startTime += 1.0

print (timeDL)
print (throughputDL)

x = []
y = []

with open(args.cwnd, "r") as f2:
    for idx, line in enumerate(f2):
        [cwnd, time] = line.split(',')
        y.append(float(cwnd.strip())); x.append(idx+int(time.strip()))   

fig=plt.figure()
ax=fig.add_subplot(111, label="1")
ax.plot(timeDL, throughputDL, color="g")
ax2=fig.add_subplot(111, label="2", frame_on=False)
ax2.plot(x, y, color="r")



# p1 = ax.plot(300, max(throughputDL), color = 'g')
# p2 = ax.plot(20, max(y), color = 'r')
# twin_stacked = ax.twiny().twinx()
# twin1 = twin_stacked.plot(timeDL, throughputDL, color = 'g')
# twin2 = twin_stacked.plot(x, y, color = 'r')
# twin_stacked.set_xlim([160000, 170000], right=True)
# plt.plot(timeDL, throughputDL, lw=2, color='r')

plt.ylabel("Throughput (Mbps)")
plt.xlabel("Time (s)")
# plt.xlim([0,300])
plt.grid(True, which="both")
plt.savefig(args.dir+'/throughput.pdf',dpi=1000,bbox_inches='tight')

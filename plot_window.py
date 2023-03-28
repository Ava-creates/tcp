import matplotlib.pyplot as plt

x = []
window_size = []
ssthresh = []

with open("CWND.csv", "r") as fp:
    fp.readline()
    for line in fp:
        data = line.strip().split(",")
        window_size.append(float(data[0]))
        x.append(float(data[1]))
        ssthresh.append(int(data[2]))

fig, ax1 = plt.subplots()
fig.set_figwidth(100)
ax2 = ax1.twinx()

# plot first set of data
ax1.plot(x, window_size, "g-")
ax1.set_ylim([min(window_size), max(window_size)])
#ax1.set_ylim([0, 100])
# plot second set of data
ax2.step(x, ssthresh, "r--")
ax2.set_ylim([min(ssthresh), max(ssthresh)])
ax2.set_xlim([min(x),max(x)])
ax1.set_xlim([min(x),max(x)])
# ax1.set_xlim([min(x),min(x)+10])


ax1.set_ylabel("Window Size (num. packets)")
ax1.set_xlabel("Time (seconds)")
ax2.set_ylabel("ss_thresh (num. packets)")

    # set colours for each y-axis to tell them apart
ax1.yaxis.label.set_color("g")
ax2.yaxis.label.set_color("r")
plt.show()
# plt.savefig("../graphs/cwnd.pdf")


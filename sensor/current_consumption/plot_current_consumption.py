import gzip
import csv
import scipy.signal
import numpy as np 
import matplotlib.pyplot as plt


def plot_current_consumption(filename, plot_title, time_span, ylim):
    current = []
    time = []
    with gzip.open(filename, 'rt') as f:
        reader = csv.reader(f)
        next(reader)
        for row in reader:
            time.append(float(row[0]))
            current.append(float(row[1]))
    current = np.asarray(current)
    time = np.asarray(time)

    idx_span = (np.searchsorted(time, time_span[0]), np.searchsorted(time, time_span[1]))

    time = time[idx_span[0]:idx_span[1]]
    current = current[idx_span[0]:idx_span[1]]

    time = scipy.signal.decimate(time, 13*1)
    current = scipy.signal.decimate(current, 13*1)

    consumed_charge = np.sum(current) * (time[1] - time[0])
    
    x = time*1e3
    y = current * 1e3
    fig = plt.figure(figsize=(8, 5), dpi=150)
    #plt.plot(x,y)
    plt.fill_between(x, y, color="skyblue", alpha=0.4)
    plt.plot(x, y, color="skyblue")

    plt.title(plot_title + f' / consumued: {consumed_charge:.2E} C')
    plt.ylim(ylim[0]*1e3, ylim[1]*1e3)
    plt.xlabel('Time [ms]')
    plt.ylabel('Current [mA]')
    plt.grid()
    plt.savefig('fig/' + plot_title + '.png')
    #plt.show()


plot_current_consumption('data/advertising.csv.gz', 'advertising', (-1e-3, 7e-3), (-0.5e-3, 16e-3))
plot_current_consumption('data/sensor_measuring1.csv.gz', 'measuring', (-1e-3, 9e-3), (-0.2e-3, 5e-3))
plot_current_consumption('data/advertising_and_measuring.csv.gz', 'advertising and measuring', (-8.2, 8.2), (-0.5e-3, 16e-3))
plot_current_consumption('data/connecting.csv.gz', 'connecting', (-0.25, 0.25), (-0.5e-3, 16e-3))

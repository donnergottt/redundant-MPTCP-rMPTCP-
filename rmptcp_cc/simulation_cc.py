#!/usr/bin/python3


from tkinter import *
from tkinter import ttk 
import subprocess
import threading
import binascii


def set_values():
	if  not(len(entry_i0.get()) == 0 or  entry_i0.get()[0] == 'b'):
		st1 = entry_i0.get()
		st2 = entry_i0_delay.get()
		st3 = entry_i0_jitter.get()
		st4 = entry_i0_ratio.get()
		st5 = entry_i0_loss.get()
		st6 = entry_i0_rate.get()
		subprocess.run(['tc', 'qdisc', 'change', 'dev', st1, 'root', 'netem', 'delay', st2 + 'ms', st3 + 'ms', st4 + '%', 'loss', st5 + '%', 'rate', st6 + 'kbit'])

	if  not(len(entry_i1.get()) == 0 or  entry_i1.get()[0] == 'b'):
		st1 = entry_i1.get()
		st2 = entry_i1_delay.get()
		st3 = entry_i1_jitter.get()
		st4 = entry_i1_ratio.get()
		st5 = entry_i1_loss.get()
		st6 = entry_i1_rate.get()
		subprocess.run(['tc', 'qdisc', 'change', 'dev', st1, 'root', 'netem', 'delay', st2 + 'ms', st3 + 'ms', st4 + '%', 'loss', st5 + '%', 'rate', st6 + 'kbit'])

	if  not(len(entry_i2.get()) == 0 or  entry_i2.get()[0] == 'b'):
		st1 = entry_i2.get()
		st2 = entry_i2_delay.get()
		st3 = entry_i2_jitter.get()
		st4 = entry_i2_ratio.get()
		st5 = entry_i2_loss.get()
		st6 = entry_i2_rate.get()
		subprocess.run(['tc', 'qdisc', 'change', 'dev', st1, 'root', 'netem', 'delay', st2 + 'ms', st3 + 'ms', st4 + '%', 'loss', st5 + '%', 'rate', st6 + 'kbit'])

def init():
	st1 = entry_i0.get()
	st2 = entry_i0_delay.get()
	st3 = entry_i0_jitter.get()
	st4 = entry_i0_ratio.get()
	st5 = entry_i0_loss.get()
	st6 = entry_i0_rate.get()
	subprocess.run(['tc', 'qdisc', 'add', 'dev', st1, 'root', 'netem', 'delay', st2 + 'ms', st3 + 'ms', st4 + '%', 'loss', st5 + '%', 'rate', st6 + 'kbit'])

	st1 = entry_i1.get()
	st2 = entry_i1_delay.get()
	st3 = entry_i1_jitter.get()
	st4 = entry_i1_ratio.get()
	st5 = entry_i1_loss.get()
	st6 = entry_i1_rate.get()
	subprocess.run(['tc', 'qdisc', 'add', 'dev', st1, 'root', 'netem', 'delay', st2 + 'ms', st3 + 'ms', st4 + '%', 'loss', st5 + '%', 'rate', st6 + 'kbit'])
	
	st1 = entry_i2.get()
	st2 = entry_i2_delay.get()
	st3 = entry_i2_jitter.get()
	st4 = entry_i2_ratio.get()
	st5 = entry_i2_loss.get()
	st6 = entry_i2_rate.get()
	subprocess.run(['tc', 'qdisc', 'add', 'dev', st1, 'root', 'netem', 'delay', st2 + 'ms', st3 + 'ms', st4 + '%', 'loss', st5 + '%', 'rate', st6 + 'kbit'])


root = Tk()
root.title("Simulation Control Center")

mainframe = ttk.Frame(root, padding="0 2 12 12")
mainframe.grid(column=0, row=0, sticky=(N, W, E, S))
mainframe.columnconfigure(0, weight=1)
mainframe.rowconfigure(0, weight=1)


simframe = ttk.Frame(mainframe, padding="4 8 12 12")
simframe.grid(column=0, row=0, sticky=W)
simframe.columnconfigure(0, weight=0, minsize=0)
simframe.rowconfigure(0, weight=1)


entry_i0 = StringVar()
entry_i0_delay= StringVar()
entry_i0_jitter = StringVar()
entry_i0_ratio = StringVar()
entry_i0_loss = StringVar()
entry_i0_rate = StringVar()
entry_i1 = StringVar()
entry_i1_delay= StringVar()
entry_i1_jitter = StringVar()
entry_i1_ratio= StringVar()
entry_i1_loss = StringVar()
entry_i1_rate = StringVar()
entry_i2 = StringVar()
entry_i2_delay= StringVar()
entry_i2_jitter = StringVar()
entry_i2_ratio= StringVar()
entry_i2_loss = StringVar()
entry_i2_rate = StringVar()

entry_i0.set('enp1s0')
entry_i0_delay.set('0')
entry_i0_jitter.set('0')
entry_i0_ratio.set('0')
entry_i0_loss.set('0')
entry_i0_rate.set('100000')
entry_i1.set('enxa0cec8029069')
entry_i1_delay.set('0')
entry_i1_jitter.set('0')
entry_i1_ratio.set('0')
entry_i1_loss.set('0')
entry_i1_rate.set('100000')
entry_i2.set('enxa0cec8052e50')
entry_i2_delay.set('0')
entry_i2_jitter.set('0')
entry_i2_ratio.set('0')
entry_i2_loss.set('0')
entry_i2_rate.set('100000')


sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i0)

sim_Entry.grid(column=2, row=2, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i0_delay)
sim_Entry.grid(column=3, row=2, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i0_jitter)
sim_Entry.grid(column=4, row=2, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i0_ratio)
sim_Entry.grid(column=5, row=2, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i0_loss)
sim_Entry.grid(column=6, row=2, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i0_rate)
sim_Entry.grid(column=7, row=2, sticky=(W, E))


sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i1)
sim_Entry.grid(column=2, row=3, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i1_delay)
sim_Entry.grid(column=3, row=3, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i1_jitter)
sim_Entry.grid(column=4, row=3, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i1_ratio)
sim_Entry.grid(column=5, row=3, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i1_loss)
sim_Entry.grid(column=6, row=3, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i1_rate)
sim_Entry.grid(column=7, row=3, sticky=(W, E))


sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i2)
sim_Entry.grid(column=2, row=4, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i2_delay)
sim_Entry.grid(column=3, row=4, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i2_jitter)
sim_Entry.grid(column=4, row=4, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i2_ratio)
sim_Entry.grid(column=5, row=4, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i2_loss)
sim_Entry.grid(column=6, row=4, sticky=(W, E))

sim_Entry = ttk.Entry(simframe, width=8, textvariable=entry_i2_rate)
sim_Entry.grid(column=7, row=4, sticky=(W, E))


ttk.Button(simframe, text="Set", command=set_values).grid(column=7, row=5, sticky=E)

ttk.Label(simframe, text="int0").grid(column=1, row=2, sticky=W)
ttk.Label(simframe, text="int1").grid(column=1, row=3, sticky=W)
ttk.Label(simframe, text="int2").grid(column=1, row=4, sticky=W)
ttk.Label(simframe, text="Label").grid(column=2, row=1, sticky=W)
ttk.Label(simframe, text="Delay (ms)").grid(column=3, row=1, sticky=W)
ttk.Label(simframe, text="Jitter (ms)").grid(column=4, row=1, sticky=W)
ttk.Label(simframe, text="Jitter %").grid(column=5, row=1, sticky=W)
ttk.Label(simframe, text="Loss %").grid(column=6, row=1, sticky=W)
ttk.Label(simframe, text="Rate kbits/s").grid(column=7, row=1, sticky=W)



for child in simframe.winfo_children(): child.grid_configure(padx=5, pady=5)

sim_Entry.focus()

init()

root.mainloop()

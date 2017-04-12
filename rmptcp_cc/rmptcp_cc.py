#!/usr/bin/python3


from tkinter import *
from tkinter import ttk 
import subprocess
import threading
import binascii

def update():
	st1 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_quota'], stderr=subprocess.STDOUT)
	st2 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_remove_noshow'], stderr=subprocess.STDOUT)
	st3 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_timeout'], stderr=subprocess.STDOUT)
	st4 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_update_periode'], stderr=subprocess.STDOUT)
	st5 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_lowpass_max_wait'], stderr=subprocess.STDOUT)
	st6 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_lowpass_learn_rate'], stderr=subprocess.STDOUT)
	st7 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_enable_mask'], stderr=subprocess.STDOUT)
	st9 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_adaptivR_enable'], stderr=subprocess.STDOUT)
	st9b = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_adaptivPath_enable'], stderr=subprocess.STDOUT)
	st10 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_ca_hold'], stderr=subprocess.STDOUT)
	st12 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_main_ratio'], stderr=subprocess.STDOUT)
	st14 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_varperiod_th'], stderr=subprocess.STDOUT)
	st15 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_numinTime'], stderr=subprocess.STDOUT)
	st16 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_runPathcheck_duration'], stderr=subprocess.STDOUT)
	st17 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_collapse_th'], stderr=subprocess.STDOUT)
	st18 = subprocess.check_output(['sysctl', '-n', 'net.mptcp.rmptcp_enable_kick'], stderr=subprocess.STDOUT)
	entry_quota.set(st1[:-1])
	entry_quota.set(st1[:-1])
	entry_noshow.set(st2[:-1])
	entry_timeout.set(st3[:-1])
	entry_update.set(st4[:-1])
	entry_lowpass.set(st5[:-1])
	entry_learn.set(st6[:-1])
	mask = hex(int(st7))
	entry_mask.set(mask)
	entry_adaptivR_enable.set(st9[:-1])
	entry_adaptivPath_enable.set(st9b[:-1])
	entry_ca_hold.set(st10[:-1])
	entry_main.set(st12[:-1])
	entry_loss_ratio.set(st14[:-1])
	entry_inTime.set(st15[:-1])
	entry_checkDuration.set(st16[:-1])
	entry_collapseth.set(st17[:-1])
	entry_kick.set(st18[:-1])


def set_values():
	if  not(len(entry_quota.get()) == 0 or  entry_quota.get()[0] == 'b'):
		st1 = entry_quota.get()
		st2 = 'net.mptcp.rmptcp_quota=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_noshow.get()) == 0 or  entry_noshow.get()[0] == 'b'):
		st1 = entry_noshow.get()
		st2 = 'net.mptcp.rmptcp_remove_noshow=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_timeout.get()) == 0 or  entry_timeout.get()[0] == 'b'):
		st1 = entry_timeout.get()
		st2 = 'net.mptcp.rmptcp_timeout=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_update.get()) == 0 or  entry_update.get()[0] == 'b'):
		st1 = entry_update.get()
		st2 = 'net.mptcp.rmptcp_update_periode=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_lowpass.get()) == 0 or  entry_lowpass.get()[0] == 'b'):
		st1 = entry_lowpass.get()
		st2 = 'net.mptcp.rmptcp_lowpass_max_wait=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_learn.get()) == 0 or  entry_learn.get()[0] == 'b'):
		st1 = entry_learn.get()
		st2 = 'net.mptcp.rmptcp_lowpass_learn_rate=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_mask.get()) == 0 or  entry_mask.get()[0] == 'b'):
		st1 = entry_mask.get()
		st2 = 'net.mptcp.rmptcp_enable_mask=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_adaptivR_enable.get()) == 0 or  entry_adaptivR_enable.get()[0] == 'b'):
		st1 = entry_adaptivR_enable.get()
		st2 = 'net.mptcp.rmptcp_adaptivR_enable=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_adaptivPath_enable.get()) == 0 or  entry_adaptivPath_enable.get()[0] == 'b'):
		st1 = entry_adaptivPath_enable.get()
		st2 = 'net.mptcp.rmptcp_adaptivPath_enable=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_ca_hold.get()) == 0 or  entry_ca_hold.get()[0] == 'b'):
		st1 = entry_ca_hold.get()
		st2 = 'net.mptcp.rmptcp_ca_hold=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_main.get()) == 0 or  entry_main.get()[0] == 'b'):
		st1 = entry_main.get()
		st2 = 'net.mptcp.rmptcp_main_ratio=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_loss_ratio.get()) == 0 or  entry_loss_ratio.get()[0] == 'b'):
		st1 = entry_loss_ratio.get()
		st2 = 'net.mptcp.rmptcp_varperiod_th=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_inTime.get()) == 0 or  entry_inTime.get()[0] == 'b'):
		st1 = entry_inTime.get()
		st2 = 'net.mptcp.rmptcp_numinTime=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_checkDuration.get()) == 0 or  entry_checkDuration.get()[0] == 'b'):
		st1 = entry_checkDuration.get()
		st2 = 'net.mptcp.rmptcp_runPathcheck_duration=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_collapseth.get()) == 0 or  entry_collapseth.get()[0] == 'b'):
		st1 = entry_collapseth.get()
		st2 = 'net.mptcp.rmptcp_collapse_th=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	if  not (len(entry_kick.get()) == 0 or  entry_kick.get()[0] == 'b'):
		st1 = entry_kick.get()
		st2 = 'net.mptcp.rmptcp_enable_kick=' + st1
		subprocess.check_output(['sysctl', '-w', st2], stderr=subprocess.STDOUT)

	update()


def test_path():
	subprocess.check_output(['sysctl', '-w', 'net.mptcp.rmptcp_runPathcheck=1'], stderr=subprocess.STDOUT)


def set_path():
	subprocess.check_output(['sysctl', '-w', 'net.mptcp.rmptcp_setPath=1'], stderr=subprocess.STDOUT)
	update()


root = Tk()
root.title("rMPTCP Control Center")

mainframe = ttk.Frame(root, padding="0 2 20 20")
mainframe.grid(column=0, row=0, sticky=(N, W, E, S))
mainframe.columnconfigure(0, weight=1)
mainframe.rowconfigure(0, weight=1)


sysctlframe = ttk.Frame(mainframe, padding="4 8 12 12")
sysctlframe.grid(column=1, row=0, sticky=W)
sysctlframe.columnconfigure(0, weight=0, minsize=0)
sysctlframe.rowconfigure(0, weight=1)

procframe = ttk.Frame(mainframe, padding="1 1 20 20")
procframe.grid(column=0, row=0, sticky=(N, W, E, S))
procframe.columnconfigure(0, weight=1)
procframe.rowconfigure(0, weight=1)

entry_quota = StringVar()
entry_noshow = StringVar()
entry_timeout = StringVar()
entry_update= StringVar()
entry_lowpass =StringVar()
entry_learn = StringVar()
entry_mask = StringVar()
text_proc = StringVar()
entry_cwr = StringVar()
entry_adaptivR_enable = StringVar()
entry_adaptivPath_enable = StringVar()
entry_ca_hold = StringVar()
entry_main = StringVar()
entry_loss_ratio = StringVar()
entry_inTime = StringVar();
entry_checkDuration = StringVar();
entry_collapseth = StringVar();
entry_kick = StringVar();


red_Entry = ttk.Entry(sysctlframe, width=8, textvariable=entry_quota)
red_Entry.grid(column=2, row=1, sticky=(W, E))

red_Entry = ttk.Entry(sysctlframe, width=8, textvariable=entry_noshow)
red_Entry.grid(column=2, row=2, sticky=(W, E))

red_Entry = ttk.Entry(sysctlframe, width=8, textvariable=entry_timeout)
red_Entry.grid(column=2, row=3, sticky=(W, E))

red_Entry = ttk.Entry(sysctlframe, width=8, textvariable=entry_update)
red_Entry.grid(column=2, row=4, sticky=(W, E))

red_Entry = ttk.Entry(sysctlframe, width=8, textvariable=entry_mask)
red_Entry.grid(column=2, row=5, sticky=(W, E))

red_Entry = ttk.Checkbutton(sysctlframe, variable=entry_adaptivR_enable, onvalue="1", offvalue="0")
red_Entry.grid(column=2, row=6, sticky=W)

red_Entry = ttk.Checkbutton(sysctlframe, variable=entry_adaptivPath_enable, onvalue="1", offvalue="0")
red_Entry.grid(column=2, row=7, sticky=W)

red_Entry = ttk.Entry(sysctlframe, width=8, textvariable=entry_ca_hold)
red_Entry.grid(column=2, row=8, sticky=(W, E))

red_Entry = ttk.Entry(sysctlframe, width=8, textvariable=entry_main)
red_Entry.grid(column=2, row=9, sticky=(W, E))

red_Entry =ttk.Entry(sysctlframe, width=8, textvariable=entry_loss_ratio)
red_Entry.grid(column=2, row=10, sticky=(W, E))

red_Entry =ttk.Entry(sysctlframe, width=8, textvariable=entry_inTime)
red_Entry.grid(column=2, row=11, sticky=(W, E))

red_Entry =ttk.Entry(sysctlframe, width=8, textvariable=entry_collapseth)
red_Entry.grid(column=2, row=12, sticky=(W, E))

red_Entry =ttk.Entry(sysctlframe, width=8, textvariable=entry_checkDuration)
red_Entry.grid(column=2, row=13, sticky=(W, E))

red_Entry = ttk.Checkbutton(sysctlframe, variable=entry_kick, onvalue="1", offvalue="0")
red_Entry.grid(column=2, row=14, sticky=W)



ttk.Button(sysctlframe, text="Reset", command=update).grid(column=1, row=15, sticky=E)
ttk.Button(sysctlframe, text="Set", command=set_values).grid(column=2, row=15, sticky=E)
ttk.Button(sysctlframe, text="Test Path", command=test_path).grid(column=3, row=15, sticky=E)
ttk.Button(sysctlframe, text="Set Path", command=set_path).grid(column=4, row=15, sticky=E)


proc_out = Text(procframe, height=40, width=100)
proc_out.grid(column=1, row=1, sticky=(N, E))

ttk.Label(sysctlframe, text="Subflow Quota Q (%)").grid(column=1, row=1, sticky=W)
ttk.Label(sysctlframe, text="Remove Subflow after (ms)").grid(column=1, row=2, sticky=W)
ttk.Label(sysctlframe, text="Max. Timeout (ms)").grid(column=1, row=3, sticky=W)
ttk.Label(sysctlframe, text="Socket Update Periode (ms)").grid(column=1, row=4, sticky=W)
ttk.Label(sysctlframe, text="Enable Mask").grid(column=1, row=5, sticky=W)
ttk.Label(sysctlframe, text="Adaptive R Enable").grid(column=1, row=6, sticky=W)
ttk.Label(sysctlframe, text="Adaptive Path Enable").grid(column=1, row=7, sticky=W)
ttk.Label(sysctlframe, text="Congestion State Hold").grid(column=1, row=8, sticky=W)
ttk.Label(sysctlframe, text="Main Ratio").grid(column=1, row=9, sticky=W)
ttk.Label(sysctlframe, text="Var Periode (%)").grid(column=1, row=10, sticky=W)
ttk.Label(sysctlframe, text="Num In Time").grid(column=1, row=11, sticky=W)
ttk.Label(sysctlframe, text="Collapse Threshold (%)").grid(column=1, row=12, sticky=W)
ttk.Label(sysctlframe, text="Test Duration per Path (s)").grid(column=1, row=13, sticky=W)
ttk.Label(sysctlframe, text="Enable Removing extra Subflow").grid(column=1, row=14, sticky=W)

for child in sysctlframe.winfo_children(): child.grid_configure(padx=5, pady=5)
for child in procframe.winfo_children(): child.grid_configure(padx=5, pady=5)

red_Entry.focus()
root.bind('<Return>', update)


def update_proc():
	threading.Timer(0.2, update_proc).start()
	tx=subprocess.check_output(['cat', '/proc/net/mptcp_net/rmptcp'], stderr=subprocess.STDOUT)
	proc_out.delete('1.0', END)
	proc_out.insert(END, tx)



update()
update_proc()
root.mainloop()

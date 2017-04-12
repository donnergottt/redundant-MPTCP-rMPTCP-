/*
 *	rMPTCP Implementation
 *
 *	(c) 2017 University Duisburg-Essen - Fachgebiet fuer Technische Informatik
 *
 *	Design & Implementation
 *  Pascal Klein <pascal.klein@uni-due.de>
 *	Martin Verbunt <martin.verbunt@stud.uni-due.de>
 *
 *
 *  Additional Authors:
 *  Marco Tesch
 *  David Schellenburg
 *  Nicolas Dammin
 *
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#ifndef _RMPTCP_H
#define _RMPTCP_H


// Redundant Output Sockets
#define MAX_NUM_RED_SK 24
#define SIZE_EBW_ARR 20
#define SIZE_R_ARR 100


enum red_sk_state {
	RED_SK_Active = 0,
#define REDF_SK_Active	(1<<RED_SK_Active)
	RED_SK_Available = 1,
#define REDF_SK_Available (1<<RED_SK_Available)
	RED_SK_Enabled = 2,
#define REDF_SK_Enabled	(1<<RED_SK_Enabled)
	RED_SK_Main = 3,
#define REDF_SK_Main (1<<RED_SK_Main)
	RED_SK_Cong = 4,
#define REDF_SK_Cong (1<<RED_SK_Cong)
	RED_SK_Sub = 5,
#define REDF_SK_Sub (1<<RED_SK_Sub)
};

enum red_state{
	RED_Open = 0,
#define REDF_Open (1 << RED_Open)
	RED_Recovery = 1,
#define REDF_Recovery (1 << RED_Recovery)
	RED_Timeout = 2,
#define REDF_Timeout (1 << RED_Timeout)
	RED_Cong = 3,
#define REDF_Cong (1 << RED_Cong)
};

#define RED_SADDR __sk_common.skc_rcv_saddr
#define RED_DADDR __sk_common.skc_daddr


void pathChecker(ktime_t *run_start);
int rmptcp_send_next_segment(struct sock *meta_sk, struct sk_buff *skb, int reinject, unsigned int mss_now);

void rmptcp_set_all_main(void);
void rmptcp_set_enable_flag(void);

/**
  * rMPTCP representation of a subflows
  * @*sk: the socket to the subflow
  * @state: state of the subflow: active, available, enabled, main, congested, substitute
  * @last_used: time of last usage of the subflow
  * @last_cong: time of last congestion event
  * @seg_count: number of segements sent via this subflow
  * @byte_count: current number of bytes sent via this subflow
  * @byte_count_old: number of bytes sent untill the last bandwidth calculation (needed for bandwidth calculation)
  * @bandwidth: current bandwidth
  * @bandwidth_mean: average bandwidth
  * @send_period: duration between the last two segments sent via this subflow
  * @send_period_mean: average send period
  * @send_period_var: variance of the send period
  * @loss_ratio: ratio of lost segments in permille, VERY VAGUE
*/
struct red_sk {
	struct sock 	*sk;
	__u8 			state:6;
	ktime_t 		last_used;
	ktime_t 		last_cong;
	u64 		 	seg_count;
	u64 		 	byte_count;
	u64 		 	byte_count_old;
	u64 			bandwidth;
	u64 			bandwidth_mean;
	s64 			send_period;
	s64 			send_period_mean;
	s64 			send_period_var;
	unsigned int 	loss_ratio;
};

/**
  * representation of the combined subflows in rMPTCP
  * @meta_sk: The meta socket of the connection
  * @state: state of the overall rMPTCP connection: open, recovery, timeout, congestion
  * @cong_flag: flag for a recent congestion, enter the conmgestion state!
  * @last_ca_state: time of the last congestion event
  * @last_update: time of the last update of bandwidth, main, enabled etc.
  * @last_inDecrease: time of last adaption/regulation of R/Q
  * @start_Recovery: time of entering the recovery state
  * @last_send: time of the last segment sent via any subflow
  * @send_period: duration between the last two segments sent via any subflow
  * @send_period_mean: average send period
  * @send_period_var: variance of the send period
  * @send_period_max: maximum  (but still normal) sending period
  * @send_period_max_before_cong: maxiumum send period measured before the last congestion state
  * @last_period_max: time of last adaption of the sending period may
  * @previous_skevent_i: subflow index, which occured the latest congestion event on
  * @timeout: flag & duration of the last timeout in us
  * @inTime_cnt: number of segments sent in time after a first timeout
  * @timeout_start: time of the first try to send the current segment
  * @total_bw: summed up current bandwidth of all subflows
  * @total_bw_mean: average total bandwidth
  * @effective_util: effective utilization, combined bandwidth of all subflows,
  					 ignoring redundantly sent segments. The bandwidth the application sees
  * @effective_util_mean: average effective utilization
  * @effective_util_mean_mean: average of the average effective utilization
  * @effective_util_var: variance of the effective utilization
  * @effective_util_mean_before_cong: average of the average effective utilization,
  									  before the last congestion state
  * @effective_byte_count: sum of all sent bytes, ignoring redundant bytes
  * @effective_byte_count_old: sum of all sent bytes, ignoring redundant bytes
  							   untill the last bandwidth calculation
  							   (needed for bandwidth calculation)
  * @sk_reg_update_period: update period of the redundant_socket_register
  						   (synchronized with sysctl_rmptcp_skreg_update) in ns
  * @max_sk_noshow: maximum duration a subflow can be unavailable untill it is removed
  					(synchronized with sysctl_rmptcp_maxnoshow) in ms
  * @red_timeout: user defind maximum timeout (synchronized with sysctl_rmptcp_timeout), in ms
  * @enable_mask: user defined enable mas to select path combination
  				  (synchronized with  sysctl_rmptcp_enablemask)
  * @collapse_th: allowed drop in effective bandwitdh in %,
  				  otherwise the congestion state is enterd
  * @q_control: current Q_control value
  * @q_control_high: The ceiling value for Q_control
  * @q_control_low: The floor value for Q_control
  * @q_actual: measured Q_actual
  * @q_target: Q_target, can be manipulated in the adaptive redundancy algorithm
  * @q_target_user: user defined Q_target, (synchronized with sysctl_rmptcp_quota)
  * @sk_list_size: number of know subflows
  * @num_enabled: number of enabled subflows
  * @sk_list: list of all subflows
*/

struct redundant_socket_register {
	struct sock 	*meta_sk;
	__u8 			state:4;
	unsigned int 	cong_flag;
	ktime_t 		last_ca_state;
	ktime_t 		last_update;
	ktime_t 		last_inDecrease;
	ktime_t			start_Recovery;

	// Failure Detection
	// Send period etc
	ktime_t 		last_send;
	s64 		 	send_period;
	s64 			send_period_mean;
	s64 			send_period_var;
	s64 			send_period_max;
	s64 			send_period_max_before_cong;
	ktime_t			last_period_max;
	unsigned int 	previous_skevent_i;
	unsigned int 	timeout;
	unsigned int 	inTime_cnt;
	ktime_t 		timeout_start;

	// Measurements
	u64   		 	total_bw;
	u64   		 	total_bw_mean;
	u64   		 	effective_util;
	u64   		 	effective_util_mean;
	u64   		 	effective_util_mean_mean;
	u64   		 	effective_util_var;
	u64   		 	effective_util_mean_before_cong;
	u64   		 	effective_byte_count;
	u64   		 	effective_byte_count_old;

	//Sysctl Variables
	int 			sk_reg_update_period;
	u64 			max_sk_noshow;
	int 			red_timeout;
	int 			enable_mask;
	int 			collapse_th;

	// Redundancy Settings
	unsigned int 	q_control;
	unsigned int 	q_control_high;
	unsigned int 	q_control_low;
 	unsigned int 	q_actual;
	unsigned int 	q_target;
	unsigned int 	q_target_user;

	// Subflow Management
	unsigned int 	sk_list_size;
	unsigned int 	num_enabled;
	struct red_sk 	sk_list[MAX_NUM_RED_SK];
};


extern struct redundant_socket_register red_sk_reg;


/*
 *  Structures for the Path Checker
 */
#define MAX_NUM_SRC_SK 4
#define MAX_NUM_DST_SK 4

/**
  * measurements belonging to a path combination
  * @mask: the path mask of the path combination
  * @lowest_RTT: the lowest subflow RTT within the combination
  * @RTT_sum: sum of all RTT in the selection
  * @total_bw: summed up bandwidth of all subflows in the combination
  * @min_subflow_bw: minimal subflow bandwidth in the combination (the bottleneck)
*/
struct pathMeasurements
{
	int 			mask;

	unsigned int 	lowest_RTT;
	unsigned int 	RTT_sum;

	unsigned int 	total_bw;
	unsigned int 	min_subflow_bw;
};

/**
  * Index of all possible path combinations in the current rMPTCP session
  * @num_src_sk: number of ip-addresses/NICs at the source side
  * @num_dst_sk: number of ip-addresses/NICs at the destination side
  * @num_pathCombinations: number of found overlap-free path combinations
  * @max_num_parallelPaths: therotical maximum of parallel paths within a overlap-free path combination
  * @active: indicates the activated combination
  * @src_addr: list of all source ip-addresses
  * @dst_addr: list of all destination ip-addresses
  * @pM: the index with measurements of all identified path combinations
  *
*/
struct pathIndex
{
	unsigned int 				num_src_sk;
	unsigned int 				num_dst_sk;
	unsigned int 				num_pathCombinations;
	unsigned int 				max_num_parallelPaths;
	int 						active;
	__be32						src_addr[MAX_NUM_SRC_SK];
	__be32						dst_addr[MAX_NUM_DST_SK];
	struct pathMeasurements 	pM[MAX_NUM_RED_SK];
};


extern struct pathIndex pI;






/*
 * 	 Redundant Input Sockets
 */
#define MAX_NUM_RED_IN_SK 100
#define ONE_SEC 1000000000
#define MICRO2NS 1000
#define NS2MILLI 1000000


struct red_in_sk {
	struct sock *sk;
	u32 dsn;
	bool dsn_send;
};

struct rmptcp_lowpass_queue {
	int queue_head;
	int queue_tail;
	struct red_in_sk sk_list[MAX_NUM_RED_IN_SK];
};


extern u64 learned_timeout; // in ns


extern int sysctl_rmptcp_quota;
extern int sysctl_rmptcp_skreg_update;
extern int sysctl_rmptcp_maxnoshow;
extern int sysctl_rmptcp_timeout;
extern int sysctl_rmptcp_lowpassmaxwait;
extern int sysctl_rmptcp_lplearnrate;
extern int sysctl_rmptcp_enablemask;
extern int sysctl_rmptcp_CS_hold;
extern int sysctl_rmptcp_adaptivR_enable;
extern int sysctl_rmptcp_adaptivPath_enable;
extern int sysctl_rmptcp_main_ratio;
extern int sysctl_rmptcp_varperiod_th;
extern int sysctl_rmptcp_numinTime;
extern int sysctl_rmptcp_runpathcheck;
extern int sysctl_rmptcp_runpathcheck_duration;
extern int sysctl_rmptcp_setPath;
extern int sysctl_rmptcp_collapse_th;
extern int sysctl_rmptcp_enable_kick;
extern int sysctl_rmptcp_minDeltaT;


bool mptcp_skb_entail(struct sock *sk, struct sk_buff *skb, int reinject);

#endif /* _RMPTCP_H */
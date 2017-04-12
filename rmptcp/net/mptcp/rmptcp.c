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

#include <linux/kconfig.h>
#include <linux/skbuff.h>
#include <linux/tcp.h>

#include <net/mptcp.h>
#include <net/rmptcp.h>
#include <net/mptcp_v4.h>
#include <net/mptcp_v6.h>
#include <net/sock.h>

struct redundant_socket_register red_sk_reg = {
		.meta_sk = NULL,
		.last_update = 0,
		.sk_list_size = 0,
		.effective_byte_count = 0,
		.effective_byte_count_old = 0,
		.effective_util_mean = 0,
		.q_actual = 0,
		.state = REDF_Open,
		.cong_flag = 0
	};


/*
 * 	Copied from Round Robin scheduler
 *	Check, if a subsocket is available right now
 */
static unsigned int rmptcp_is_available(const struct sock *sk, const struct sk_buff *skb,
				  bool zero_wnd_test, bool cwnd_test)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	unsigned int space, in_flight;
	struct red_sk *rsk = (struct red_sk *) sk;

	/* Set of states for which we are allowed to send data */
	if (!mptcp_sk_can_send(sk))
	{
		return 1;
	}

	/* We do not send data on this subflow unless it is
	 * fully established, i.e. the 4th ack has been received.
	 */
	if (tp->mptcp->pre_established)
	{
		return 2;
	}

	if (tp->pf)
	{
		return 3;
	}

	if (inet_csk(sk)->icsk_ca_state == TCP_CA_Loss) {
		/* If SACK is disabled, and we got a loss, TCP does not exit
		 * the loss-state until something above high_seq has been acked.
		 * (see tcp_try_undo_recovery)
		 *
		 * high_seq is the snd_nxt at the moment of the RTO. As soon
		 * as we have an RTO, we won't push data on the subflow.
		 * Thus, snd_una can never go beyond high_seq.
		 */
		if (!tcp_is_reno(tp))
		{
			return 4;
		}
		else if (tp->snd_una != tp->high_seq)
		{
			return 5;
		}
	}

	if ( !tp->mptcp->fully_established ) {
		/* Make sure that we send in-order data */
		if (skb && tp->mptcp->second_packet &&
		    tp->mptcp->last_end_data_seq != TCP_SKB_CB(skb)->seq)
			{
				return 6;
			}
	}

	if (!cwnd_test)
		goto zero_wnd_test;

	in_flight = tcp_packets_in_flight(tp);
	/* Not even a single spot in the cwnd */
	if (in_flight >= tp->snd_cwnd)
	{
		return 7;
	}

	/* Now, check if what is queued in the subflow's send-queue
	 * already fills the cwnd.
	 */
	space = (tp->snd_cwnd - in_flight) * tp->mss_cache;

	if (tp->write_seq - tp->snd_nxt > space)
	{
		return 8;
	}

zero_wnd_test:
	if (zero_wnd_test && !before(tp->write_seq, tcp_wnd_end(tp)))
	{
		return 9;
	}

	return 0;
}


/*
 * insert socket to socket register in a sorted way
 */
void rmptcp_insert_sk(ktime_t *run_start, struct sock *red_sk)
{
	__be32 saddr = red_sk->RED_SADDR;
	__be32 daddr = red_sk->RED_DADDR;
	int i, j;

	if( unlikely(red_sk_reg.sk_list_size == 0) )
	{
		i = 0;
		red_sk_reg.sk_list_size++;
	}
	else
	{
		// find correct (sorted) position to insert
		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			if (saddr > red_sk_reg.sk_list[i].sk->RED_SADDR)
			{
				continue;
			}
			else if ( (saddr == red_sk_reg.sk_list[i].sk->RED_SADDR) && (daddr < red_sk_reg.sk_list[i].sk->RED_DADDR) )
			{
				break;
			}
			else if ( (saddr == red_sk_reg.sk_list[i].sk->RED_SADDR) && (daddr > red_sk_reg.sk_list[i].sk->RED_DADDR) )
			{
				continue;
			}
			//should not happen
			else if ( (saddr == red_sk_reg.sk_list[i].sk->RED_SADDR) && (daddr == red_sk_reg.sk_list[i].sk->RED_DADDR) )
			{
				break;
			}
			else if (saddr < red_sk_reg.sk_list[i].sk->RED_SADDR)
			{
				break;
			}
		}

		red_sk_reg.sk_list_size++;

		for (j = red_sk_reg.sk_list_size - 1; j > i; j--)
		{
			red_sk_reg.sk_list[j] = red_sk_reg.sk_list[j-1];
		}
	}

	red_sk_reg.sk_list[i].sk = red_sk;
	red_sk_reg.sk_list[i].last_used = *run_start;
	red_sk_reg.sk_list[i].seg_count = 0;
	red_sk_reg.sk_list[i].byte_count = 0;
	red_sk_reg.sk_list[i].byte_count_old = 0;
	red_sk_reg.sk_list[i].state = (REDF_SK_Main | REDF_SK_Active);
}

/*
 *	Set the enable flag according to the given path mask
 */
void rmptcp_set_enable_flag(void)
{
	unsigned int num_disabled = 0;
	int i;

	// set enable flag
	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		if ( red_sk_reg.enable_mask  & (1 << i) )
		{
			red_sk_reg.sk_list[i].state |= REDF_SK_Enabled;
		}
		else
		{
			// if the socket is not enabled, it cannot be a main socket either
			red_sk_reg.sk_list[i].state &= ~(REDF_SK_Enabled | REDF_SK_Main);
		}
	}

	// count disabled sk, for correct mesh
 	for ( i = 0; i < red_sk_reg.sk_list_size; i++)
 	{
 		if ( !( red_sk_reg.enable_mask  & (1 << i) ) )
 		{
 			num_disabled++;
 		}
 	}

 	red_sk_reg.num_enabled = red_sk_reg.sk_list_size - num_disabled;
}


/*
 * Search for new Sockets and add them
 */
void rmptcp_update_sk_list(ktime_t *run_start, struct tcp_sock *meta_tp, struct sk_buff *skb)
{
 	int sk_is_known = 0;
	struct sock *red_sk;
	int i;

	if (red_sk_reg.state & REDF_Cong)
	{
		return;
	}

	mptcp_for_each_sk(meta_tp->mpcb, red_sk)
	{
		sk_is_known = 0;
		if (rmptcp_is_available(red_sk, skb, false, 1))
		{
			continue;
		}
		// Do we already know the socket?
		else
		{
			for (i = 0; i < red_sk_reg.sk_list_size; i++){
				if(red_sk_reg.sk_list[i].sk == red_sk){
					sk_is_known = 1;
					break;
				}
			}
			// New Socket, add to record
			if (sk_is_known == 0){
				rmptcp_insert_sk(run_start, red_sk);
			}
		}
	}

	rmptcp_set_enable_flag();

	return;
}


/*
 *	Set all enabled/substitute Subflows as main subflows
 */
void rmptcp_set_all_main(void)
{
	int i;

	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		if( red_sk_reg.sk_list[i].state & (REDF_SK_Enabled | REDF_SK_Sub) )
		{
			red_sk_reg.sk_list[i].state |= REDF_SK_Main;
		}
		else
		{
			red_sk_reg.sk_list[i].state &= ~REDF_SK_Main;
		}
	}
}

/*
 *	Set all enabled/substitute/uncongested subflows as main subflows
 */
void rmptcp_set_all_uncong_main(void)
{
	int i;
	int c = 0;

	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		if(  (red_sk_reg.sk_list[i].state & (REDF_SK_Enabled | REDF_SK_Sub) ) &&
		    !(red_sk_reg.sk_list[i].state & REDF_SK_Cong) )
		{
			red_sk_reg.sk_list[i].state |= REDF_SK_Main;
			c++;
		}
		else
		{
			red_sk_reg.sk_list[i].state &= ~REDF_SK_Main;
		}
	}

	// if all subflows are congested set them all as main
	if (c == 0)
	{
		rmptcp_set_all_main();
	}
}

/*
 *	Set the main flag
 */
void rmptcp_update_main_sk(void)
{
	u32 lowest_RTT = U32_MAX;
	unsigned int highest_bw = 0;
	struct tcp_sock *sub_tp;
	int i;
	int num_main = 0;

	// during recovery we need all subflows to be main,
	// otherwise the bandwidth can collapse
	if (red_sk_reg.state & REDF_Recovery)
	{
		rmptcp_set_all_main();
		return;
	}

	else if (red_sk_reg.state & REDF_Cong)
	{
		rmptcp_set_all_uncong_main();
		return;
	}


	// Get socket with lowest RTT and highest Bandwidth
	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		if( ( red_sk_reg.sk_list[i].state & (REDF_SK_Enabled | REDF_SK_Sub) )  && !(red_sk_reg.sk_list[i].state & REDF_SK_Cong) )
		{
			sub_tp = tcp_sk(red_sk_reg.sk_list[i].sk);
			if (sub_tp->srtt_us < lowest_RTT)
			{
				lowest_RTT = sub_tp->srtt_us;
			}
			if (red_sk_reg.sk_list[i].bandwidth_mean > highest_bw)
			{
				highest_bw = red_sk_reg.sk_list[i].bandwidth_mean;
			}
		}
	}

	// Set Main Subflows
	// Depending on Bandwidth and RTT
	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		if( red_sk_reg.sk_list[i].state & (REDF_SK_Enabled | REDF_SK_Sub) )
		{
			sub_tp = tcp_sk(red_sk_reg.sk_list[i].sk);

			// unmark as main
			// small hysteresis (+1)
			if ( (sub_tp->srtt_us > (sysctl_rmptcp_main_ratio + 1) * lowest_RTT) ||
						(red_sk_reg.sk_list[i].bandwidth_mean < (highest_bw/(sysctl_rmptcp_main_ratio + 1)) )  ||
						(red_sk_reg.sk_list[i].state & REDF_SK_Cong) )
			{
				red_sk_reg.sk_list[i].state &= ~REDF_SK_Main;
			}
			// set as main
			else if (sub_tp->srtt_us < sysctl_rmptcp_main_ratio * lowest_RTT &&
						(red_sk_reg.sk_list[i].bandwidth_mean > (highest_bw/sysctl_rmptcp_main_ratio) ) )
			{
				red_sk_reg.sk_list[i].state |= REDF_SK_Main;
				num_main++;
			}
			// we do not change anything, but we have to account for existing main sockets
			else if(red_sk_reg.sk_list[i].state & REDF_SK_Main)
			{
				num_main++;
			}
		}
	}

	// If no Main could be established, make all subflows main subflows
	if (!num_main)
	{
		rmptcp_set_all_uncong_main();
	}

}

/*
 *	Measure the Bandwith of all Subflows and/or the total Bandwidth/Utilization
 *
 * 	All mean calculation are done using the division by 16 or 32. These values are purly
 *	experimental and were evaulated by trial and error. So there is absolutly no gurantee,
 *	that these values are good or even the best in the respective context.
 */
void rmptcp_update_bw(ktime_t *run_start, unsigned int calc_subflows)
{
	int i;
	s64 err;
	s64 delta_T;
	struct tcp_sock *sub_tp;


	delta_T = ktime_us_delta(*run_start, red_sk_reg.last_update);

	/* time period has been too short
		values <20ms have shown a unstable calculation of bandwidth
	*/
	if (delta_T < sysctl_rmptcp_minDeltaT)
	{
		return;
	}


	// calc Bandwidth of the subflows
	if (calc_subflows)
	{
		red_sk_reg.total_bw = 0;
		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			red_sk_reg.sk_list[i].bandwidth = (u64) (
				( ( red_sk_reg.sk_list[i].byte_count - red_sk_reg.sk_list[i].byte_count_old ) * 8 * 1000) // 8: byte, 1000: kilo
				/ delta_T  );
			red_sk_reg.sk_list[i].byte_count_old  = red_sk_reg.sk_list[i].byte_count;
			red_sk_reg.total_bw += red_sk_reg.sk_list[i].bandwidth;

			err = (s64) red_sk_reg.sk_list[i].bandwidth;
			err -= red_sk_reg.sk_list[i].bandwidth_mean;
			red_sk_reg.sk_list[i].bandwidth_mean = red_sk_reg.sk_list[i].bandwidth_mean + (err/16);


			// loss ratio
			// Attention: VERY vague
			sub_tp = tcp_sk(red_sk_reg.sk_list[i].sk);
			if (sub_tp->packets_out > 0)
			{
				err = (s64) sub_tp->lost_out*1000/sub_tp->packets_out; // 1000: in permille
				err -= red_sk_reg.sk_list[i].loss_ratio;
				if (err > -8 && err < 0)
				{
					red_sk_reg.sk_list[i].loss_ratio--;
				}
				else
				{
					red_sk_reg.sk_list[i].loss_ratio = red_sk_reg.sk_list[i].loss_ratio + (err/8);
				}
			}
			else if (!(red_sk_reg.sk_list[i].state & REDF_SK_Enabled) )
			{
				red_sk_reg.sk_list[i].loss_ratio = 0;
			}
		}

		//highly inspired by the van jacobsen RTT estimator
		err = (s64) red_sk_reg.total_bw;
		err -= red_sk_reg.total_bw_mean;
		red_sk_reg.total_bw_mean = red_sk_reg.total_bw_mean + err/16;
	}
	else
	{
		// no bandwidth calculation, but reset byte count so the next calculation will be correct
		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			red_sk_reg.sk_list[i].byte_count_old  = red_sk_reg.sk_list[i].byte_count;

			// loss ratio
			sub_tp = tcp_sk(red_sk_reg.sk_list[i].sk);
			if (sub_tp->packets_out > 0)
			{
				err = (s64) sub_tp->lost_out*1000/sub_tp->packets_out; // 1000: in permille
				err -= red_sk_reg.sk_list[i].loss_ratio;
				red_sk_reg.sk_list[i].loss_ratio = red_sk_reg.sk_list[i].loss_ratio + (err/16);
			}
			else if (!(red_sk_reg.sk_list[i].state & REDF_SK_Enabled) )
			{
				red_sk_reg.sk_list[i].loss_ratio = 0;
			}
		}
	}

	// calc effective total bandwidth
	red_sk_reg.effective_util = (u64) (
		(red_sk_reg.effective_byte_count - red_sk_reg.effective_byte_count_old) * 8 * 1000)
		/ delta_T;
	red_sk_reg.effective_byte_count_old = red_sk_reg.effective_byte_count;
	red_sk_reg.last_update = *run_start;


	err = (s64) red_sk_reg.effective_util;
	err -= red_sk_reg.effective_util_mean;
	red_sk_reg.effective_util_mean = red_sk_reg.effective_util_mean + err/16;


	// calculation of the mean mean value is only done in the OPEN State,
	// if we calculated the mean mean value during other states, the measuremnt
	// would influence the failure detection
	if ( red_sk_reg.state & REDF_Open )
	{
		err = (s64) red_sk_reg.effective_util_mean;
		err -= red_sk_reg.effective_util_mean_mean;
		red_sk_reg.effective_util_mean_mean = red_sk_reg.effective_util_mean_mean + err/32;
		if (err < 0 )
		{
			err = -err;
		}
		err -= red_sk_reg.effective_util_var;
		red_sk_reg.effective_util_var += err/32;
	}
	// During recovery with deactived adaptive Redundancy the mean mean value can also be calculated
	else if ( (red_sk_reg.state & REDF_Recovery) && !sysctl_rmptcp_adaptivPath_enable )
	{
		err = (s64) red_sk_reg.effective_util_mean;
		err -= red_sk_reg.effective_util_mean_mean;
		red_sk_reg.effective_util_mean_mean = red_sk_reg.effective_util_mean_mean + err/16;
		if (err < 0 )
		{
			err = -err;
		}
		err -= red_sk_reg.effective_util_var;
		red_sk_reg.effective_util_var += err/32;
	}

	return;
}

/*
 *	In the congestion State with activated adaptive Path
 *	the redundancy is regulated here to fullfil the bandwidth needs
 */
void rmptcp_regulateQ(ktime_t *run_start)
{
	if ( ktime_before(ktime_add_ms(red_sk_reg.last_inDecrease, 20), *run_start ) )
	{
		red_sk_reg.last_inDecrease =  *run_start;

		// in the recovery state slowly force the redundancy up again
		if (red_sk_reg.state & REDF_Recovery)
		{
			red_sk_reg.q_target++;
		}
		// The utilization is  1.05 times higher than before the congestion: Higher Q!
		else if( red_sk_reg.effective_util_mean > ((red_sk_reg.effective_util_mean_before_cong*21)/20) &&
			red_sk_reg.effective_util      > ((red_sk_reg.effective_util_mean_before_cong*21)/20) )
		{
			if (red_sk_reg.q_actual > red_sk_reg.q_target)
			{
				red_sk_reg.q_target++;
				red_sk_reg.q_target += ((red_sk_reg.q_actual-red_sk_reg.q_target) >> 4);
			}
			else
			{
				red_sk_reg.q_target++;
			}
		}
		// Otherwise lower Q
		else if( red_sk_reg.effective_util_mean < red_sk_reg.effective_util_mean_before_cong )
		{
			red_sk_reg.q_target--;
		}

		// it has to be at least lower than the now total available subflows
		if (red_sk_reg.state & REDF_Cong)
		{
			if (sysctl_rmptcp_adaptivPath_enable)
			{
				if ( red_sk_reg.q_target > red_sk_reg.num_enabled*100 )
				{
					red_sk_reg.q_target = red_sk_reg.num_enabled*100;
				}
			}
			else
			{
				if ( red_sk_reg.q_target > (red_sk_reg.num_enabled - 1)*100 )
				{
					red_sk_reg.q_target = (red_sk_reg.num_enabled - 1)*100;
				}
			}
		}
	}
}

/*
 *  State Change to the Congestion State
 */
void rmptcp_statechange_cong(int subflow, ktime_t *run_start)
{
	red_sk_reg.state = REDF_Cong;
	red_sk_reg.cong_flag = 1;
	red_sk_reg.last_ca_state = *run_start;
	red_sk_reg.sk_list[subflow].state |= REDF_SK_Cong;
}

/*
 *  State change from Timeout to congestion
 */
void rmptcp_statechange_timeout2cong(ktime_t *run_start)
{
	int i;
	int sk_event_i = 0;
	s64 sk_timeout = S64_MAX;
	s64 sk_nxt = 0;

	printk("Failure detection by Timeout: %i inTime: %i\n", red_sk_reg.send_period, red_sk_reg.inTime_cnt);

	/*
	 * So, at this point a timeout has occured (again) and we need to know
	 * which subflow is responsible for it:
	 * We check all enabled but unavailable subflows and check which
	 * one should probable be available right now.
	 * (I.e smallest Last Used + Send_Period + a * Send_Var)
	*/
	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		if ( (red_sk_reg.sk_list[i].state & REDF_SK_Enabled) && !(red_sk_reg.sk_list[i].state & REDF_SK_Available ) )
		{
			sk_nxt = ktime_to_us(red_sk_reg.sk_list[i].last_used) +
						red_sk_reg.sk_list[i].send_period_mean +
						(((100 + sysctl_rmptcp_varperiod_th) * red_sk_reg.sk_list[i].send_period_var)/100);
			if ( sk_timeout > sk_nxt )
			{
				sk_timeout = sk_nxt;
				sk_event_i = i;
			}
		}
	}

	// no sk identified, choose the one, which sent longest time ago
	if (sk_timeout == S64_MAX)
	{
		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			if ( (red_sk_reg.sk_list[i].state & REDF_SK_Enabled) && !(red_sk_reg.sk_list[i].state & REDF_SK_Available ) )
			{
				sk_nxt = ktime_to_us(red_sk_reg.sk_list[i].last_used);
				if ( sk_timeout > sk_nxt )
				{
					sk_timeout = sk_nxt;
					sk_event_i = i;
				}
			}
		}
	}

	rmptcp_statechange_cong(sk_event_i, run_start);
}

/*
 *  State Change after Sk Event to Congestion
 */
void rmptcp_statechange_event2cong(int sk_event_i, ktime_t *run_start)
{
	rmptcp_statechange_cong(sk_event_i, run_start);
	printk("Failure detection by effective utilization drop: %i, before %i\n", red_sk_reg.effective_util_mean, red_sk_reg.effective_util_mean_before_cong);
}

/*
 *	Check for a subflow failure relevant to rMPTCP
 * 	The Congestion State is entered if a timeout occured two times
 *	within a small period of time
*/
void rmptcp_check_failure(ktime_t *run_start)
{
	int i;
	struct inet_connection_sock *icsk;
	int sk_event = 0;
	int sk_event_i = 0;


	// check for congestion event on sockets
	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		icsk = inet_csk(red_sk_reg.sk_list[i].sk);
		if ( icsk->icsk_ca_state >= TCP_CA_Recovery )
		{
			// get the socket with the highest CA Value, this one is most
			// likely to be responsible for a collapse in effective Utilization
			if( (icsk->icsk_ca_state > sk_event) && (red_sk_reg.sk_list[i].state & REDF_SK_Enabled) )
			{
				sk_event = icsk->icsk_ca_state;
				sk_event_i = i;
			}
			// Substitutes can also get an congested state, so they will
			// not be marked as main if congested
			if ( (red_sk_reg.state == REDF_Cong) &&
				 (red_sk_reg.sk_list[i].state & REDF_SK_Sub) &&
				 (icsk->icsk_ca_state > TCP_CA_Recovery)
				)
			{
				red_sk_reg.sk_list[i].state |= REDF_SK_Cong;
				red_sk_reg.sk_list[i].last_cong = *run_start;
			}
		}
		else
		{
			// if substitutes can get the congested state they also can get uncongested
			// after a period of congestion free sending
			if ( (red_sk_reg.state == REDF_Cong) && (red_sk_reg.sk_list[i].state & REDF_SK_Sub) &&
				(ktime_before(ktime_add_ms(red_sk_reg.sk_list[i].last_cong, sysctl_rmptcp_CS_hold), *run_start ) ) )
			{
				red_sk_reg.sk_list[i].state &= ~REDF_SK_Cong;
			}
		}
	}

	// The State Machine
	switch(red_sk_reg.state)
	{
		case REDF_Open:
		{
			// First tactic: Check for timeouts
			// First Timeout, enter Timeout State
			if (red_sk_reg.timeout)
 			{
 				//high timeout? directly go to congested
				if  (red_sk_reg.timeout > 2*red_sk_reg.send_period_max)
				{
					rmptcp_statechange_timeout2cong(run_start);
				}
				else
				{
					red_sk_reg.state = REDF_Timeout;
					red_sk_reg.send_period_max_before_cong = red_sk_reg.send_period_max;
				}
			}
			// Second tactic: Check if the utilization decreases rapdily
			else if ((sk_event >= TCP_CA_Recovery) &&
				(red_sk_reg.effective_util_mean < (red_sk_reg.effective_util_mean_before_cong*red_sk_reg.collapse_th)/100 ) )
			{
				rmptcp_statechange_event2cong(sk_event_i, run_start);
				red_sk_reg.previous_skevent_i = sk_event_i;
			}
			else
			{
				red_sk_reg.effective_util_mean_before_cong = red_sk_reg.effective_util_mean_mean;
			}
			break;
		}

		case REDF_Recovery:
		{
			// First tactic: Check for timeouts
			// First Timeout, enter Timeout State
			if (red_sk_reg.timeout)
 			{
 				//high timeout? directly go to congested
				if  (red_sk_reg.timeout > 2*red_sk_reg.send_period_max)
				{
					rmptcp_statechange_timeout2cong(run_start);
				}
				else
				{
					red_sk_reg.state = REDF_Timeout;
					red_sk_reg.send_period_max_before_cong = red_sk_reg.send_period_max;
				}
			}
			// Second tactic: Check if the utilization decreases rapdily
			else if ((sk_event >= TCP_CA_Recovery) &&
					(red_sk_reg.effective_util_mean < (red_sk_reg.effective_util_mean_before_cong*red_sk_reg.collapse_th)/100 ) )
			{
				rmptcp_statechange_event2cong(sk_event_i, run_start);
				red_sk_reg.previous_skevent_i = sk_event_i;
			}
			// Third recovery tactic: Check if the utilization decreases rapdily independent of a current socket event
			else if (red_sk_reg.effective_util_mean < (red_sk_reg.effective_util_mean_before_cong*red_sk_reg.collapse_th)/100 )
			{
				rmptcp_statechange_event2cong(red_sk_reg.previous_skevent_i, run_start);
			}
			// extend stay in recovery, if only a congestion event occured
			else if( sk_event == TCP_CA_Loss )
			{
				red_sk_reg.state = REDF_Recovery;
				red_sk_reg.start_Recovery = *run_start;
			}
			// exit recovery state, after an failure free hold period
			else if ( ktime_before(ktime_add_ms(red_sk_reg.start_Recovery, sysctl_rmptcp_CS_hold*2), *run_start ) )
			{
				red_sk_reg.state = REDF_Open;
				red_sk_reg.cong_flag = 1;

				for (i = 0; i < red_sk_reg.sk_list_size; i++)
				{
					red_sk_reg.sk_list[i].state &= ~REDF_SK_Cong;
				}
			}
			else if (!sk_event && (ktime_before(ktime_add_ms(red_sk_reg.start_Recovery, sysctl_rmptcp_CS_hold), *run_start ) ) )
			{
				red_sk_reg.effective_util_mean_before_cong = red_sk_reg.effective_util_mean_mean;
			}
			break;
		}

		case REDF_Timeout:
		{
			// Second Timeout, enter Congestion State
			if (red_sk_reg.timeout)
			{
				rmptcp_statechange_timeout2cong(run_start);
			}
			// No second timeout, return to recovery state
			else if (red_sk_reg.inTime_cnt >= sysctl_rmptcp_numinTime)
			{
				red_sk_reg.inTime_cnt = 0;
				red_sk_reg.state = REDF_Recovery;
				red_sk_reg.start_Recovery = *run_start;
				red_sk_reg.last_period_max = *run_start;
				red_sk_reg.send_period_max = red_sk_reg.send_period_max_before_cong;
			}
			// congestion detected by collapse in bandiwdth
			else if ((sk_event >= TCP_CA_Recovery) &&
				(red_sk_reg.effective_util_mean < (red_sk_reg.effective_util_mean_before_cong*red_sk_reg.collapse_th)/100 ) )
			{
				rmptcp_statechange_event2cong(sk_event_i, run_start);
				red_sk_reg.previous_skevent_i = sk_event_i;
			}
			break;
		}

		case REDF_Cong:
		{
			/* To exit the congestion state we do not look at timeouts, but at the congestion state
			 * of the failed subflow. If the subflow was 'healthy' for a specified period of time
			 * The congestion state is left and the recovery state is entered
			*/
			if ( ktime_before(ktime_add_ms(red_sk_reg.last_ca_state, sysctl_rmptcp_CS_hold), *run_start ) )
			{
				red_sk_reg.state = REDF_Recovery;
				red_sk_reg.send_period_max = red_sk_reg.send_period_max_before_cong;
				red_sk_reg.start_Recovery = *run_start;
				red_sk_reg.last_period_max = *run_start;
				red_sk_reg.cong_flag = 1;
				if ( (red_sk_reg.effective_util_mean < red_sk_reg.effective_util_mean_mean) &&
					 (red_sk_reg.effective_util_mean > 0) )
				{
					red_sk_reg.effective_util_mean_mean = red_sk_reg.effective_util_mean;
				}

				for (i = 0; i < red_sk_reg.sk_list_size; i++)
				{
					red_sk_reg.sk_list[i].state &= ~REDF_SK_Cong;
				}
			}
			// again found a congestion event? Extend congestion state!
			else if( sk_event == TCP_CA_Loss )
			{
				red_sk_reg.last_ca_state = *run_start;
			}
			break;
		}
		default:
		{
			printk("Error rMPTCP: Invalid RED State\n");
			red_sk_reg.state = REDF_Open;
			break;
		}
	}
}

/*
 *	Calculate the control values for Q
 */
void rmptcp_setQ(void)
{
	red_sk_reg.q_target = red_sk_reg.q_target_user;
	// calculate control values
	red_sk_reg.q_control_low = (int) red_sk_reg.q_target/100;
	red_sk_reg.q_control_high = ( (int) (red_sk_reg.q_target - 1)/100 ) + 1;
}


/*
 *	Apply the adaptive redundancy algorithm
 */
void rmptcp_adaptR(ktime_t *run_start)
{
	unsigned int sk_event_i = 0;
	unsigned int i;

	// after entering Congestion state
	if(red_sk_reg.cong_flag && (red_sk_reg.state & REDF_Cong) )
	{
		// get congested subflow first
		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			if (red_sk_reg.sk_list[i].state & REDF_SK_Cong)
			{
				sk_event_i = i;
				break;
			}
		}

		red_sk_reg.last_inDecrease = *run_start;
		if (red_sk_reg.effective_util_mean_before_cong < 1)
		{
			red_sk_reg.effective_util_mean_before_cong = 1;
		}
		// calculate new adapted target
		red_sk_reg.q_target =
			((red_sk_reg.total_bw_mean - red_sk_reg.sk_list[sk_event_i].bandwidth_mean) * 100) /
			red_sk_reg.effective_util_mean_before_cong;

		// it has to be at least lower than the now total available subflows
		if ( red_sk_reg.q_target > (red_sk_reg.num_enabled - 1)*100 )
		{
			red_sk_reg.q_target = (red_sk_reg.num_enabled - 1)*100;
		}
	}
	// after transition from recovery state to open state
	else if (red_sk_reg.cong_flag && (red_sk_reg.state & REDF_Open) )
	{
		rmptcp_setQ();
	}
	// always regulate R in congested state
	else if( (red_sk_reg.state & REDF_Cong) || (red_sk_reg.state & REDF_Recovery) )
	{
		rmptcp_regulateQ(run_start);
	}
	else
	{
		red_sk_reg.q_target = red_sk_reg.q_target_user;
	}


	//last sanity check
	if (red_sk_reg.q_target < 100)
	{
		red_sk_reg.q_target = 100;
	}
	else if (red_sk_reg.q_target > red_sk_reg.q_target_user)
	{
		red_sk_reg.q_target = red_sk_reg.q_target_user;
	}

	// calculate control value
	red_sk_reg.q_control_low = (int) red_sk_reg.q_target/100;
	red_sk_reg.q_control_high = ( (int) (red_sk_reg.q_target - 1)/100 ) + 1;

	return;
}

/*
 * 	Apply the adaptive Path algortihm
 */
void rmptcp_adaptPath(void)
{
	__be32 saddr_cong, daddr_cong, saddr_new, daddr_new;
	unsigned int sk_cong_i = 0;
	unsigned int i = 0;
	unsigned int highest_bw = 0;
	unsigned int highest_bw_i = 0;

	// after entering Congestion state
	if(red_sk_reg.cong_flag && (red_sk_reg.state & REDF_Cong) )
	{
		// get congested subflow
		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			if ( red_sk_reg.sk_list[i].state & REDF_SK_Cong )
			{
				sk_cong_i = i;
				break;
			}
		}

		// addresses of the congested subflow
		saddr_cong = red_sk_reg.sk_list[sk_cong_i].sk->RED_SADDR;
		daddr_cong = red_sk_reg.sk_list[sk_cong_i].sk->RED_DADDR;


		// get socket with highest BW
		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			if( (red_sk_reg.sk_list[i].state & (REDF_SK_Enabled | REDF_SK_Cong)) == (REDF_SK_Enabled & ~REDF_SK_Cong) )
			{
				if (red_sk_reg.sk_list[i].bandwidth_mean > highest_bw)
				{
					highest_bw = red_sk_reg.sk_list[i].bandwidth_mean;
					highest_bw_i = i;
				}
			}
		}

		// Set substitute path (Take the path originating from the interfaces with the highest Bandwidth)
		saddr_new = red_sk_reg.sk_list[highest_bw_i].sk->RED_SADDR;
		daddr_new = red_sk_reg.sk_list[highest_bw_i].sk->RED_DADDR;

		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			if (
				!(red_sk_reg.sk_list[i].state & REDF_SK_Cong) &&
				(
				 ( (red_sk_reg.sk_list[i].sk->RED_SADDR == saddr_cong) && (red_sk_reg.sk_list[i].sk->RED_DADDR == daddr_new) ) ||
				 ( (red_sk_reg.sk_list[i].sk->RED_DADDR == daddr_cong) && (red_sk_reg.sk_list[i].sk->RED_SADDR == saddr_new) )
				)
			   )
			{
				red_sk_reg.sk_list[i].state |= REDF_SK_Sub;
			}
		}
	}
	// Leaving the congestion state into the recover state
	// Reset all substitute subflows to inactive
	else if (red_sk_reg.cong_flag && (red_sk_reg.state & REDF_Recovery) )
	{
		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			red_sk_reg.sk_list[i].state &= ~REDF_SK_Sub;
		}
	}
}

/*
 *  Measure the actual Q
 */
void rmptcp_setQActual(unsigned int num_available_sk)
{
	int err = 0;

	if (red_sk_reg.q_actual != 0)
	{
		err = (int) num_available_sk;
		err *= 100;
		err -= red_sk_reg.q_actual;

		// make sure the actual value can reach is real value
		if ( (err < 16) && (err > 0) )
		{
			red_sk_reg.q_actual++;
		}
		else if ( (err > -16) && (err < 0) )
		{
			red_sk_reg.q_actual--;
		}
		else
		{
			red_sk_reg.q_actual += err/16;
		}
	}
	else
	{
		red_sk_reg.q_actual = num_available_sk * 100;
	}
}

/*
 *	Check within the known sockets which are available right now
 *	Decide whether we got enough to send
 */
bool rmptcp_check_avail_sk(struct sk_buff *skb, ktime_t *run_start)
{
	int i, j;
	int num_available_sk = 0;
	int err = 0;
	unsigned int go = 0;
	/* 0 = do not set
	/  1 = send, target reached or timeout
	 */

 	int latest_i = -1;
 	unsigned int main_available = 0;

	s64 delta_send;

	struct tcp_sock *sub_tp;


	// delta_send is the time needed between the first try and the actual sending
	// of a segment
	if (red_sk_reg.timeout_start.tv64 != 0)
	{
		delta_send = ktime_us_delta(*run_start, red_sk_reg.timeout_start);
	}
	else
	{
		delta_send = 0;
	}


	// check known sockets for availability
	for (i = 0; i < red_sk_reg.sk_list_size; i++){
		sub_tp = tcp_sk(red_sk_reg.sk_list[i].sk);

		if (rmptcp_is_available(red_sk_reg.sk_list[i].sk, skb, false, 1))
		{
			red_sk_reg.sk_list[i].state &= ~REDF_SK_Available;
			// we have not seen the socket in a long time, remove it
			if ( ktime_before(ktime_add_ms(red_sk_reg.sk_list[i].last_used, red_sk_reg.max_sk_noshow), *run_start ) )
			{
				for (j = i; j <  red_sk_reg.sk_list_size-1; j++)
				{
					red_sk_reg.sk_list[j] = red_sk_reg.sk_list[j+1];
				}
				red_sk_reg.sk_list_size--;
				i--;
			}
		}
		// available socket
		else
		{
			/* we use available sockets if
			 * 	- they are enabled or a substitute AND
			 *	- if they are additionally a substitute and congested they are
			 *		only allowed to have one packet unacknowledged "out"
			 */
			if ((red_sk_reg.sk_list[i].state & (REDF_SK_Enabled | REDF_SK_Sub) )
				&&
				!(  (red_sk_reg.sk_list[i].state & REDF_SK_Cong) &&
					(red_sk_reg.sk_list[i].state & REDF_SK_Sub) &&
					(sub_tp->packets_out > 0)
				)
			   )
			{
				red_sk_reg.sk_list[i].state |= REDF_SK_Available;
				num_available_sk++;
				if(red_sk_reg.sk_list[i].state & REDF_SK_Main)
				{
					main_available++;
				}

			}
			else
			{
				red_sk_reg.sk_list[i].state &= ~REDF_SK_Available;
			}
		}
	}

	//make sure the target is possible
	if (red_sk_reg.q_control > red_sk_reg.num_enabled )
	{
		red_sk_reg.q_control = red_sk_reg.num_enabled;
	}

	// but it has to be at least 1
	if(red_sk_reg.q_control < 1 )
	{
		red_sk_reg.q_control = 1;
	}

	/*	we reached the target and got at least one main socket
	 *	(if we are in the congested state, all sockets are always main sockets)
	 */
	if ( (num_available_sk >= red_sk_reg.q_control) && main_available )
	{
		go = 1;

		// enabled to remove extra, unneeded subflows
		if (sysctl_rmptcp_enable_kick)
		{
			while ( (num_available_sk > red_sk_reg.q_control) && (main_available > 1) )
	 		{
				// we have more than one main socket, we can kick any main socket

				//Finding the first Main socket
				for (i = 0; i < red_sk_reg.sk_list_size; i++)
				{
					if( (red_sk_reg.sk_list[i].state & (REDF_SK_Available | REDF_SK_Main)) == (REDF_SK_Available | REDF_SK_Main) )
					{
						latest_i = i;
						break;
					}
				}

				// Comparing all main sockets regarding when they were last uses
				// The socket which was used last is removed
				for (i = latest_i+1; i < red_sk_reg.sk_list_size; i++)
				{
					if( (red_sk_reg.sk_list[i].state & (REDF_SK_Available | REDF_SK_Main)) == (REDF_SK_Available | REDF_SK_Main) )
					{
						if(ktime_before(red_sk_reg.sk_list[latest_i].last_used, red_sk_reg.sk_list[i].last_used))
						{
							latest_i = i;
						}
					}
				}

				// Removing the socket from the available list
				if (latest_i >= 0)
				{
					red_sk_reg.sk_list[latest_i].state &= ~REDF_SK_Available;
					num_available_sk--;
					main_available--;
				}
				else
				{
					break;
				}
			}
		}
	}
	//Timeout in Congestion State
	else if ( 	(red_sk_reg.state & REDF_Cong) &&
				(delta_send > (((100+sysctl_rmptcp_varperiod_th) * 2 * red_sk_reg.send_period_max)/100) ) &&
				(num_available_sk > 0)  )
	{
		printk("TIMEOUT in Congestion-State\n");
		go = 1;
	}
	// Timeout
	else if (  !(red_sk_reg.state & REDF_Cong) &&
			   (delta_send > (((100+sysctl_rmptcp_varperiod_th)*red_sk_reg.send_period_max)/100) ) )
	{
		// Set flag, so a failure can be detected
		red_sk_reg.timeout = delta_send;

		// In case of timeout, send immediatly (if we got any socket)
		if(num_available_sk > 0)
		{
			go = 1;
		}
		else
		{
			go = 0;
		}
	}

	// no sockets, but timeout not reached
	// not needed in this current version
	else
	{
		// First time we got no sockets for this skb, start timer
		if (red_sk_reg.timeout_start.tv64 == 0)
		{
			red_sk_reg.timeout_start = *run_start;
		}
		go = 0;
	}

 	// We got no sockets at all
	if (num_available_sk == 0){
		go = 0;
	}

	// we have found sockets, reset timer, save actual q
	if (go)
	{
		rmptcp_setQActual(num_available_sk);
		red_sk_reg.timeout_start.tv64 = 0;
	}

 	return go;
 }

/*
 *	Set  q_control
 */
void rmptcp_calc_controlQ(void)
{
	if ( red_sk_reg.q_actual >= red_sk_reg.q_target )
	{
		red_sk_reg.q_control = red_sk_reg.q_control_low;
	}
	else
	{
		red_sk_reg.q_control = red_sk_reg.q_control_high;
	}

	if(red_sk_reg.q_control < 1)
	{
		red_sk_reg.q_control = 1;
	}
}

/*
 * 	Measure the sending period to detect timeouts
 * and therefore failures
 */
void rmptcp_calc_sendPeriod(ktime_t *run_start)
{
	s64 err;
	int i;

	/* 	This measures how long it takes for a segment from the last sent segement
	 *	to the actual sending
	 */
	red_sk_reg.send_period = ktime_us_delta(*run_start, red_sk_reg.last_send);
	red_sk_reg.last_send = *run_start;
	red_sk_reg.timeout_start.tv64 = 0;

	err =  red_sk_reg.send_period;
	err -= red_sk_reg.send_period_mean;
	red_sk_reg.send_period_mean += (err/32);

	if( err < 0 )
	{
		err = -err;
	}
	err -= red_sk_reg.send_period_var;
	red_sk_reg.send_period_var += err/64;


	/* count segemnts, which do not cause a timeout,
	 * to detect the second timeout correctly
	 * We need a second timeout within a number of segments to enter the congested
	 * state, otherwise we return to the recovery state
	 */
	if ( !red_sk_reg.timeout && (red_sk_reg.state & REDF_Timeout) )
	{
		red_sk_reg.inTime_cnt++;
	}

	// calculate the maximal sending period-
	if ( !(red_sk_reg.state & REDF_Timeout) && !red_sk_reg.timeout )
	{
		// is the sending periode within the user specified range?
		if(
			(
			 (red_sk_reg.send_period > (red_sk_reg.send_period_max * 90)/100) &&
			 (red_sk_reg.send_period < (red_sk_reg.send_period_max * (100 + sysctl_rmptcp_varperiod_th))/100)
			)
		  )
		{
			err = red_sk_reg.send_period;
			err -= red_sk_reg.send_period_max;
			if (red_sk_reg.effective_byte_count < 1000000)
			{
				red_sk_reg.send_period_max += err/8;
			}
			else
			{
				red_sk_reg.send_period_max += err/64;
			}

			red_sk_reg.last_period_max = *run_start;
		}
		// reduce the maximum periode every second, so it can also decrease
		else if (ktime_before(ktime_add_ms(red_sk_reg.last_period_max, 1000), *run_start ))
		{
			red_sk_reg.send_period_max -= red_sk_reg.send_period_max/4;
			red_sk_reg.last_period_max = *run_start;
		}
	}

	// plausability check
	if (red_sk_reg.send_period_max < red_sk_reg.send_period_mean + red_sk_reg.send_period_var)
	{
		red_sk_reg.send_period_max = red_sk_reg.send_period_mean + red_sk_reg.send_period_var;
	}
	// limit to user maximum value
	if (red_sk_reg.send_period_max > red_sk_reg.red_timeout*1000)
	{
		red_sk_reg.send_period_max = red_sk_reg.red_timeout*1000; // in us
	}
	// make sure it is not zero
	if (red_sk_reg.send_period_max < 0)
	{
		red_sk_reg.send_period_max = 1;
	}


	//calculate send period for used subflows
	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{

		if ( !(red_sk_reg.sk_list[i].state & REDF_SK_Available) )
		{
			continue;
		}

		if ( !red_sk_reg.timeout || !(red_sk_reg.state & REDF_Timeout) )
		{
			red_sk_reg.sk_list[i].send_period = ktime_us_delta(*run_start, red_sk_reg.sk_list[i].last_used);
			if (!red_sk_reg.timeout)
			{
				err =  red_sk_reg.sk_list[i].send_period;
				err -= red_sk_reg.sk_list[i].send_period_mean;
				red_sk_reg.sk_list[i].send_period_mean += (err/64);

				if( err < 0 )
				{
					err = -err;
				}
				err -= red_sk_reg.sk_list[i].send_period_var;
				red_sk_reg.sk_list[i].send_period_var += err/64;
			}
		}
	}
}


/*
 * Send on available Sockets
 */
bool rmptcp_send_skb(struct sk_buff *skb, int *reinject, unsigned int mss_now, ktime_t *run_start)
{
	int i, ebc_check;
	struct sock *red_sk;

	for (i = 0; i < red_sk_reg.sk_list_size; i++) {

		if ( !(red_sk_reg.sk_list[i].state & REDF_SK_Available) )
		{
			continue;
		}

		red_sk = red_sk_reg.sk_list[i].sk;
		red_sk_reg.sk_list[i].state &= ~REDF_SK_Available;
		red_sk_reg.sk_list[i].last_used = *run_start;
		red_sk_reg.sk_list[i].seg_count++;
		red_sk_reg.sk_list[i].byte_count += (*skb).len;

		/* put the segment on the subflow-buffer (reinject is
		 * from one perticular subflow!)
		*/
		if (!mptcp_skb_entail(red_sk, skb, *reinject)) {
			return false;
			break;
		}

		/* Nagle is handled at the MPTCP-layer, so
		 * always push on the subflow
		 */
		__tcp_push_pending_frames(red_sk, mss_now, TCP_NAGLE_PUSH);
	}

	if(*reinject)
	{
		printk("reinject\n");
	}
	else
	{
		red_sk_reg.effective_byte_count += (*skb).len;
	}

	return true;
}

/*
 *	Set the sysctl variables to red-variables with sanity check
 */
void rmptcp_update_sysctl(void)
{
	red_sk_reg.sk_reg_update_period = sysctl_rmptcp_skreg_update * 1000000;
	if (red_sk_reg.sk_reg_update_period <= 0)
	{
		red_sk_reg.sk_reg_update_period = 1;
	}


	red_sk_reg.max_sk_noshow = (u64) sysctl_rmptcp_maxnoshow;
	if (red_sk_reg.max_sk_noshow <= 0)
	{
		red_sk_reg.max_sk_noshow = 1;
	}


	red_sk_reg.red_timeout = sysctl_rmptcp_timeout;
	if (red_sk_reg.red_timeout <= 0)
	{
		red_sk_reg.red_timeout = 1;
	}


	if (sysctl_rmptcp_quota < 100)
	{
		sysctl_rmptcp_quota = 100;
	}
	red_sk_reg.q_target_user = sysctl_rmptcp_quota;



	if (sysctl_rmptcp_setPath && (pI.num_pathCombinations > 0) )
	{
		pI.active++;
		if ( (pI.active >= pI.num_pathCombinations) || (pI.active < 0) )
		{
			pI.active = 0;
		}

		sysctl_rmptcp_enablemask = pI.pM[pI.active].mask;
		sysctl_rmptcp_setPath = 0;
	}

	red_sk_reg.enable_mask = sysctl_rmptcp_enablemask;


	if (sysctl_rmptcp_collapse_th > 0 && sysctl_rmptcp_collapse_th < 100)
	{
		red_sk_reg.collapse_th = sysctl_rmptcp_collapse_th;
	}

}

/*
 *	Reset values after a change of meta socket.
 */
void rmptcp_reset_meta(void)
{
	red_sk_reg.state = REDF_Open;
	red_sk_reg.cong_flag = 0;
	red_sk_reg.send_period = 0;
	red_sk_reg.send_period_mean = 0;
	red_sk_reg.send_period_var = 0;
	red_sk_reg.send_period_max = 0;
	red_sk_reg.sk_list_size = 0;
	red_sk_reg.num_enabled = 0;
	red_sk_reg.total_bw = 0;
	red_sk_reg.total_bw_mean = 0;
	red_sk_reg.effective_util = 0;
	red_sk_reg.effective_util_mean = 0;
	red_sk_reg.effective_util_mean_mean = 0;
	red_sk_reg.effective_util_var = 0;
	red_sk_reg.effective_util_mean_before_cong = 0;
	red_sk_reg.effective_byte_count = 0;
	red_sk_reg.effective_byte_count_old = 0;
	red_sk_reg.q_control = 0;
	red_sk_reg.q_control_high = 0;
	red_sk_reg.q_control_low = 0;
	red_sk_reg.q_actual = 0;
	red_sk_reg.q_target = 0;
	red_sk_reg.q_target_user = 0;
	red_sk_reg.send_period_max_before_cong = 50000;


	// we can not start/continue the pathchecker
	sysctl_rmptcp_runpathcheck = 0;
	// path index has to be reseted, too
	pI.num_pathCombinations = 0;
}


/*
 *	The starting point of the rMPTCP scheduler
 */
int rmptcp_send_next_segment(struct sock *meta_sk, struct sk_buff *skb, int reinject, unsigned int mss_now)
{
	ktime_t run_start;
	struct tcp_sock *meta_tp = tcp_sk(meta_sk);

	run_start = ktime_get();

	// Do we still have the same Meta Socket?
	// If not, start new
	if (red_sk_reg.meta_sk != meta_sk)
	{
		red_sk_reg.meta_sk = meta_sk;
		rmptcp_reset_meta();
	}

	// if the pathcker is run, we have another procedure
	if(sysctl_rmptcp_runpathcheck)
	{
		pathChecker(&run_start);
		rmptcp_update_bw(&run_start, 1);
		rmptcp_setQ();
		rmptcp_calc_controlQ();
	}
	// no path evaluation, run the normal scheduler
	else
	{
		// Update Socket list & sysctl variables // update each 50ms
		if (run_start.tv64 - red_sk_reg.last_update.tv64 >  red_sk_reg.sk_reg_update_period)
		{
			rmptcp_update_sysctl();
			rmptcp_update_sk_list(&run_start, meta_tp, skb);
			rmptcp_update_bw(&run_start, 1);
			rmptcp_update_main_sk();
		}

		if (sysctl_rmptcp_adaptivPath_enable)
		{
			rmptcp_adaptPath();
		}

		if (sysctl_rmptcp_adaptivR_enable)
		{
			rmptcp_adaptR(&run_start);
			rmptcp_update_main_sk();
		}
		else
		{
			rmptcp_setQ();
		}

		red_sk_reg.cong_flag = 0;
		rmptcp_calc_controlQ();
	}

	// Check for available sockets
	if (!rmptcp_check_avail_sk(skb, &run_start))
	{
		return 0;
	}


	rmptcp_calc_sendPeriod(&run_start);
	rmptcp_check_failure(&run_start);
	red_sk_reg.timeout = 0;

 	//send on available sockets
 	if (!rmptcp_send_skb(skb, &reinject, mss_now, &run_start))
	{
		return 0;
	}

	return 1;
}

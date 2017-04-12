/*
 *	rMPTCP Implementation - Path Checker
 *
 *	(c) 2017 University Duisburg-Essen - Fachgebiet fuer Technische Informatik
 *
 *	Design & Implementation
 *  Pascal Klein <pascal.klein@uni-due.de>
 *	Martin Verbunt <martin.verbunt@stud.uni-due.de>
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <net/mptcp.h>
#include <net/rmptcp.h>

struct pathIndex pI;

/*
 *	Get all available addresses in the path manager, but only once
 */

static void getAddresses(void)
{
	unsigned int i,j;

	pI.num_src_sk = 0;
	pI.num_dst_sk = 0;

	// Get source addresses
	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		for (j = 0; j < pI.num_src_sk; j++)
		{
			if( red_sk_reg.sk_list[i].sk->RED_SADDR == pI.src_addr[j] )
			{
				break;
			}
		}
		// we went through the whole pI without finding the current address:
		// so add it
		if ( j >= pI.num_src_sk )
		{
			pI.src_addr[pI.num_src_sk] = red_sk_reg.sk_list[i].sk->RED_SADDR;
			pI.num_src_sk++;
			if (pI.num_src_sk >= MAX_NUM_SRC_SK)
			{
				printk("ERROR PATH 1\n");
				return;
			}
		}
	}


	// get destination addresses
	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		for (j = 0; j < pI.num_dst_sk; j++)
		{
			if( red_sk_reg.sk_list[i].sk->RED_DADDR == pI.dst_addr[j] )
			{
				break;
			}
		}
		// we went through the whole the pI without finding the current address:
		// so add it
		if ( j >= pI.num_dst_sk )
		{
			pI.dst_addr[pI.num_dst_sk] = red_sk_reg.sk_list[i].sk->RED_DADDR;
			pI.num_dst_sk++;
			if (pI.num_dst_sk >= MAX_NUM_DST_SK)
			{
				printk("ERROR PATH 2\n");
				return;
			}
		}
	}

}

/*
 * 	Determine all possible path combination
 * 	without using any interface (i.e. ip address)
 *	more than once
 */
static void getPathCombinations(void)
{
	unsigned int 	i,j,k;
	unsigned int 	testPathMask;
	unsigned int 	maxPathMask;
	unsigned int 	numTestPaths;
	__be32			src_addr_test[MAX_NUM_SRC_SK];
	__be32		 	dst_addr_test[MAX_NUM_DST_SK];
	unsigned int 	num_src_test, num_dst_test;

	getAddresses();


	maxPathMask = (1 << red_sk_reg.sk_list_size);
	// The theoretical maximum number of parallel paths
	pI.max_num_parallelPaths = min(pI.num_src_sk, pI.num_dst_sk);
	pI.num_pathCombinations = 0;

	// we try out any possible path mask (i.e. brute force)
	for ( testPathMask = 0; testPathMask < maxPathMask; testPathMask++)
	{
		numTestPaths = 0;
		num_src_test = 0;
		num_dst_test = 0;

		// first check the number of paths in the current testPathMask
		for (i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			if ( testPathMask & (1 << i) )
			{
				numTestPaths++;
			}
		}
		// it has to be equal to the
		if (numTestPaths != pI.max_num_parallelPaths)
		{
			continue;
		}


		// check of any of the addresses in the current path Mask used more than once
		for ( i = 0; i < red_sk_reg.sk_list_size; i++)
		{
			// is this bit set?
			if ( !(testPathMask & (1 << i)) )
			{
				continue;
			}
			// check source ip address
			for ( j = 0; j < num_src_test; j++)
			{
				if ( red_sk_reg.sk_list[i].sk->RED_SADDR == src_addr_test[j] )
				{
					break;
				}
			}
			// check destination ip address
			for ( k = 0; k < num_dst_test; k++)
			{
				if ( red_sk_reg.sk_list[i].sk->RED_DADDR == dst_addr_test[k] )
				{
					break;
				}
			}
			// no matches? add it to a temporary array to check for further
			// matches in other paths of the current combination
			if ( (j >= num_src_test) && (k >= num_dst_test) )
			{
				src_addr_test[num_src_test] = red_sk_reg.sk_list[i].sk->RED_SADDR;
				dst_addr_test[num_dst_test] = red_sk_reg.sk_list[i].sk->RED_DADDR;
				num_src_test++;
				num_dst_test++;
				if (num_src_test >= MAX_NUM_SRC_SK || num_dst_test >= MAX_NUM_DST_SK)
				{
					printk("ERROR PATH 3\n");
					return;
				}

			}
			else
			{
				break;
			}
		}

		/* we iterated through the whole list without having to break
		 * so we have a path combination without overlapping paths
		 * we add it to the permanent list and continue with the next combination
		 * otherwise we continue without saving
		*/
		if ( i >= red_sk_reg.sk_list_size )
		{
			pI.pM[pI.num_pathCombinations].mask = testPathMask;
			pI.pM[pI.num_pathCombinations].lowest_RTT = 0;
			pI.pM[pI.num_pathCombinations].RTT_sum = 0;
			pI.pM[pI.num_pathCombinations].total_bw = 0;
			pI.pM[pI.num_pathCombinations].min_subflow_bw = 0;
			pI.num_pathCombinations++;
		}
	}

	return;

}

/*
 *	After the test we gather the measurement information
 *	belonging to the current path combination and save it
 */
void getMeasurements(int index)
{
	struct tcp_sock *sub_tp;
	int i;

	pI.pM[index].total_bw = red_sk_reg.total_bw_mean;
	pI.pM[index].lowest_RTT = UINT_MAX;
	pI.pM[index].RTT_sum = 0;
	pI.pM[index].min_subflow_bw = UINT_MAX;

	for (i = 0; i < red_sk_reg.sk_list_size; i++)
	{
		sub_tp = tcp_sk(red_sk_reg.sk_list[i].sk);
		if (red_sk_reg.sk_list[i].state & (REDF_SK_Enabled ))
		{
			// get lowest RTT and the RTT Sum
			pI.pM[index].RTT_sum += (sub_tp->srtt_us >> 3);
			if ((sub_tp->srtt_us >> 3) < pI.pM[index].lowest_RTT)
			{
				pI.pM[index].lowest_RTT = (sub_tp->srtt_us >> 3);
			}

			if(red_sk_reg.sk_list[i].bandwidth_mean < pI.pM[index].min_subflow_bw)
			{
				pI.pM[index].min_subflow_bw = red_sk_reg.sk_list[i].bandwidth_mean;
			}
		}
	}
}

/*
 *	Get all possible overlap free Path combinations
 * 	And measure the performance of each combination
 */
void pathChecker(ktime_t *run_start)
{
	int i;
	static ktime_t start_last_combination;
	static int running = 0;
	static int test_index;

	// Flag is freshly set, so get all possible path combinations
	// and start the test
	if (!running)
	{
		running = 1;
		test_index = 0;

		getPathCombinations();

		pI.active = -1;

		for ( i = 0; i < pI.num_pathCombinations; i++)
		{
			printk("%x\n", pI.pM[i].mask);
		}

		red_sk_reg.enable_mask = pI.pM[test_index].mask;
		rmptcp_set_enable_flag();
		rmptcp_set_all_main();
		start_last_combination = *run_start;
	}
	// During the measurement
	else
	{
		// The current measurement runs for a specified time per path
		// after that the next path is tested
		if (ktime_before(ktime_add_ms(start_last_combination, sysctl_rmptcp_runpathcheck_duration*1000), *run_start ))
		{
			getMeasurements(test_index);
			test_index++;

			// de we itarete throug all path combinations?
			// yes? than finish
			if (test_index >= pI.num_pathCombinations)
			{
				running = 0;
				sysctl_rmptcp_runpathcheck = 0;
			}
			// otherwise test next combination
			else
			{
				red_sk_reg.enable_mask = pI.pM[test_index].mask;
				rmptcp_set_enable_flag();
				rmptcp_set_all_main();
				start_last_combination = *run_start;
			}
		}

	}

	return;
}
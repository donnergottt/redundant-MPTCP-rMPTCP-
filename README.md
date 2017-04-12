# redundant-MPTCP-rMPTCP-
by Pascal A. Klein, Martin Verbunt, Axel Hunger,

University Duisburg-Essen

A redundant Scheduler for MPTCP, dynamically adding redundant data segments utilizing different subflows for improved latency 

This Scheduler is an extension for MPTCP and has several improvements regarding the standard redundant mechanisms:

- Dynamic redunancy management on a subflow and bandwidth basis
- Dynamic path management for estimating best paths and automatic recreation
- Error recognition with two different methods for failure compensation
- Out of Order avoidance by dynamic prioritization of subflows
- API implementation for adjusting parameters for rMPTCP
- Control Center Application for utilizing the API for the adjustment of parameters

An extended documentation will follow

The following papers have been published in the course of development:

”Equalizing Latency Peaks Using a Redundant Multipath-TCP Scheme,” 2016 International Conference on Information Networking (ICOIN), Kota Kinabalu, Malaysia, Jan 2016, pp. 184-189.

“Evaluation of the Redundancy-Bandwidth Trade-off and Jitter Compensation in rMPTCP,” 2016 8th IFIP International Conference on New Technology, Mobile & Security (NTMS), Larnaca, Cyprus, Nov 2016.

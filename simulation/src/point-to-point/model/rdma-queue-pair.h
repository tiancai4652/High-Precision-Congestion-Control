#ifndef RDMA_QUEUE_PAIR_H
#define RDMA_QUEUE_PAIR_H

#include <ns3/object.h>
#include <ns3/packet.h>
#include <ns3/ipv4-address.h>
#include <ns3/data-rate.h>
#include <ns3/event-id.h>
#include <ns3/custom-header.h>
#include <ns3/int-header.h>
#include <vector>
namespace ns3 {

class RdmaQueuePair : public Object {
public:
	Time startTime;
	Ipv4Address sip, dip;
	uint16_t sport, dport;
	uint64_t m_size;
	uint64_t snd_nxt, snd_una; // next seq to send, the highest unacked seq
	uint16_t m_pg;
	uint16_t m_ipid;
	uint32_t m_win; // bound of on-the-fly packets
	uint64_t m_baseRtt; // base RTT of this qp
	DataRate m_max_rate; // max rate
	bool m_var_win; // variable window size
	Time m_nextAvail;	//< Soonest time of next send
	uint32_t wp; // current window of packets
	uint32_t lastPktSize;
	Callback<void> m_notifyAppFinish;


	//skip_sim
	uint64_t ori_m_size=0;
	std::vector<double> m_rate_array;  // 存储不同时间的 m_rate 值
	// 方法：插入到 m_rate_array
    uint64_t UpdateRateArray() {
		uint64_t r = m_rate.GetBitRate();
		m_rate_array.push_back(r); // 将当前 m_rate 存入数组
		// std::cout << "m_rate_array:" << m_rate_array.size() << "\n";
		return r;
	}

	bool CheckStable(uint64_t lengthMin, uint64_t deltaMax)
	{
		// 检查 m_rate_array 长度是否满足 minimum 长度
		if (m_rate_array.size() < lengthMin)
		{
			return false;
		}
	    std::vector<double> lastElements(m_rate_array.end() - lengthMin, m_rate_array.end());

		// 获取 m_rate_array 的最大值和最小值
		double maxRate = *std::max_element(lastElements.begin(), lastElements.end());
		double minRate = *std::min_element(lastElements.begin(), lastElements.end());

		double delta = maxRate - minRate;
		std::cout <<sip.Get()<<"."<< sport << ".delta:" << delta << "\n";

		m_rate_array=lastElements;

		// 判断最大值和最小值的差是否小于等于 deltaMax
		if (delta <= deltaMax)
		{
			return true;
		}

		return false;
	}

	void StepTime(Time time, uint64_t lengthMin)
	{
		std::vector<double> lastElements(m_rate_array.end() - lengthMin, m_rate_array.end());
		m_rate_array=lastElements;
		double rate = m_rate_array[m_rate_array.size() - 1];
		double size= time.GetSeconds()*rate/8;
		if(ori_m_size==0)
		{
			ori_m_size=m_size;
		}
		uint64_t m_size_current = m_size;
		m_size -= size;
		std::cout << sip.Get() << "." << sport << ".m_size=" << m_size_current << "-" << size << "=" << m_size << std::endl;
	}

	void ClearRateArray()
	{
		m_rate_array.clear();
	}

	double GetAsumeFinishTimeS()
	{
		if(ori_m_size==0)
		{
			ori_m_size=m_size;
		}
		uint64_t size = m_size - snd_una;
		double rate = m_rate_array[m_rate_array.size() - 1] / 8;
		return size / rate;
	}
	/******************************
	 * runtime states
	 *****************************/
	DataRate m_rate;	//< Current rate
	struct {
		DataRate m_targetRate;	//< Target rate
		EventId m_eventUpdateAlpha;
		double m_alpha;
		bool m_alpha_cnp_arrived; // indicate if CNP arrived in the last slot
		bool m_first_cnp; // indicate if the current CNP is the first CNP
		EventId m_eventDecreaseRate;
		bool m_decrease_cnp_arrived; // indicate if CNP arrived in the last slot
		uint32_t m_rpTimeStage;
		EventId m_rpTimer;
	} mlx;
	struct {
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		IntHop hop[IntHeader::maxHop];
		uint32_t keep[IntHeader::maxHop];
		uint32_t m_incStage;
		double m_lastGap;
		double u;
		struct {
			double u;
			DataRate Rc;
			uint32_t incStage;
		}hopState[IntHeader::maxHop];
	} hp;
	struct{
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		uint32_t m_incStage;
		uint64_t lastRtt;
		double rttDiff;
	} tmly;
	struct{
		uint32_t m_lastUpdateSeq;
		uint32_t m_caState;
		uint32_t m_highSeq; // when to exit cwr
		double m_alpha;
		uint32_t m_ecnCnt;
		uint32_t m_batchSizeOfAlpha;
	} dctcp;
	struct{
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		uint32_t m_incStage;
	}hpccPint;

	/***********
	 * methods
	 **********/
	static TypeId GetTypeId (void);
	RdmaQueuePair(uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport, uint16_t _dport);
	void SetSize(uint64_t size);
	void SetWin(uint32_t win);
	void SetBaseRtt(uint64_t baseRtt);
	void SetVarWin(bool v);
	void SetAppNotifyCallback(Callback<void> notifyAppFinish);

	uint64_t GetBytesLeft();
	uint32_t GetHash(void);
	void Acknowledge(uint64_t ack);
	uint64_t GetOnTheFly();
	bool IsWinBound();
	uint64_t GetWin(); // window size calculated from m_rate
	bool IsFinished();
	uint64_t HpGetCurWin(); // window size calculated from hp.m_curRate, used by HPCC
};

class RdmaRxQueuePair : public Object { // Rx side queue pair
public:
	struct ECNAccount{
		uint16_t qIndex;
		uint8_t ecnbits;
		uint16_t qfb;
		uint16_t total;

		ECNAccount() { memset(this, 0, sizeof(ECNAccount));}
	};
	ECNAccount m_ecn_source;
	uint32_t sip, dip;
	uint16_t sport, dport;
	uint16_t m_ipid;
	uint32_t ReceiverNextExpectedSeq;
	Time m_nackTimer;
	int32_t m_milestone_rx;
	uint32_t m_lastNACK;
	EventId QcnTimerEvent; // if destroy this rxQp, remember to cancel this timer

	static TypeId GetTypeId (void);
	RdmaRxQueuePair();
	uint32_t GetHash(void);
};

class RdmaQueuePairGroup : public Object {
public:
	std::vector<Ptr<RdmaQueuePair> > m_qps;
	//std::vector<Ptr<RdmaRxQueuePair> > m_rxQps;

	static TypeId GetTypeId (void);
	RdmaQueuePairGroup(void);
	uint32_t GetN(void);
	Ptr<RdmaQueuePair> Get(uint32_t idx);
	Ptr<RdmaQueuePair> operator[](uint32_t idx);
	void AddQp(Ptr<RdmaQueuePair> qp);
	//void AddRxQp(Ptr<RdmaRxQueuePair> rxQp);
	void Clear(void);
};

}

#endif /* RDMA_QUEUE_PAIR_H */

#ifndef NFD_DAEMON_FW_HYBRID_VANET_STRATEGY_H
#define NFD_DAEMON_FW_HYBRID_VANET_STRATEGY_H

#include "ns3/ndnSIM/NFD/daemon/fw/multicast-strategy.h"
#include "ns3/ndnSIM/NFD/daemon/fw/retx-suppression-exponential.h"

namespace nfd {
namespace fw {

class HybridVanetStrategy : public Strategy {
public:
  explicit HybridVanetStrategy(Forwarder& forwarder, const Name& name = getStrategyName());
  static const Name& getStrategyName();

  void afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                            const shared_ptr<pit::Entry>& pitEntry) override;

private:
  // Network status functions
  bool isRsuAvailable(const fib::Entry& fibEntry) const;
  int getNeighborDensity(const fib::Entry& fibEntry) const;

// Density threshold 
static const int DENSE_THRESHOLD = 5; 

  //  Retransmission Suppression from multicast-vanet-strategy
  RetxSuppressionExponential m_retxSuppression;
  static const time::milliseconds RETX_SUPPRESSION_INITIAL;
  static const time::milliseconds RETX_SUPPRESSION_MAX;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_HYBRID_VANET_STRATEGY_H

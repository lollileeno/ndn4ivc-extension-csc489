#include "hybrid-vanet-strategy.hpp"
#include "/home/ndn4ivc/ndnSIM/ns-3/src/ndnSIM/NFD/daemon/fw/algorithm.hpp"
#include "/home/ndn4ivc/ndnSIM/ns-3/src/ndnSIM/NFD/daemon/common/logger.hpp"

namespace nfd {
namespace fw {

NFD_REGISTER_STRATEGY(HybridVanetStrategy);
NFD_LOG_INIT("HybridVanetStrategy");

const time::milliseconds HybridVanetStrategy::RETX_SUPPRESSION_INITIAL(10);
const time::milliseconds HybridVanetStrategy::RETX_SUPPRESSION_MAX(250);

const Name& HybridVanetStrategy::getStrategyName() {
  static Name strategyName("/localhost/nfd/strategy/hybrid-vanet/%FD%01");
  return strategyName;
}

HybridVanetStrategy::HybridVanetStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder),
    m_retxSuppression(RETX_SUPPRESSION_INITIAL, 
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  this->setInstanceName(makeInstanceName(name, getStrategyName()));
}

void HybridVanetStrategy::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                                               const shared_ptr<pit::Entry>& pitEntry)
{
  const fib::Entry &fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList &nexthops = fibEntry.getNextHops();
  
  // case 1: there is available RSU - directly forward to RSU
  if (isRsuAvailable(fibEntry)) {
    NFD_LOG_DEBUG("Condition A: RSU Available. Forwarding strictly to RSU.");
    for (const auto &nexthop : nexthops) {
      Face &outFace = nexthop.getFace();
      if (outFace.getScope() == ndn::nfd::FACE_SCOPE_NON_LOCAL) {
         this->sendInterest(pitEntry, FaceEndpoint(outFace, 0), interest);
         return; 
      }
    }
  }

  int currentDensity = getNeighborDensity(fibEntry);

  // case 2: dense traffic mobility - use multicast-vanet strategy
  if (currentDensity >= DENSE_THRESHOLD) {
    NFD_LOG_DEBUG("Condition B: Dense Traffic (" << currentDensity << "). Using Multicast.");
    int nEligibleNextHops = 0;
    bool isSuppressed = false;

    for (const auto &nexthop : nexthops) {
      Face &outFace = nexthop.getFace();
      RetxSuppressionResult suppressResult = m_retxSuppression.decidePerUpstream(*pitEntry, outFace);

      if (suppressResult == RetxSuppressionResult::SUPPRESS) {
        isSuppressed = true;
        continue;
      }

      if ((outFace.getId() == ingress.face.getId() && outFace.getLinkType() != ndn::nfd::LINK_TYPE_AD_HOC) ||
          wouldViolateScope(ingress.face, interest, outFace)) {
        continue;
      }

      this->sendInterest(pitEntry, FaceEndpoint(outFace, 0), interest);
      if (suppressResult == RetxSuppressionResult::FORWARD) {
        m_retxSuppression.incrementIntervalForOutRecord(*pitEntry->getOutRecord(outFace));
      }
      ++nEligibleNextHops;
    }

    if (nEligibleNextHops == 0 && !isSuppressed) {
      // Suppress NACK 
      this->rejectPendingInterest(pitEntry);
    }
    return;
  }

// case 3: sparse traffic mobility - carry and forward strategy
  if (currentDensity < DENSE_THRESHOLD) {
    NFD_LOG_DEBUG("Condition C: Sparse Traffic. Buffering Interest in PIT.");
// Store the interest packet into PIT 
    return;
  }
}

// function to estimate the density 
int HybridVanetStrategy::getNeighborDensity(const fib::Entry& fibEntry) const {
  return fibEntry.getNextHops().size(); 
}

//function to check if there is a direct path to RSU
bool HybridVanetStrategy::isRsuAvailable(const fib::Entry& fibEntry) const {
  for (const auto &nexthop : fibEntry.getNextHops()) {
    if (nexthop.getFace().getRemoteUri().toString().find("rsu") != std::string::npos) {
      return true;
    }
  }
  return false;
}

} // namespace fw
} // namespace nfd

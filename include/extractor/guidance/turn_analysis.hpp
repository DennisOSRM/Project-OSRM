#ifndef OSRM_EXTRACTOR_TURN_ANALYSIS
#define OSRM_EXTRACTOR_TURN_ANALYSIS

#include "extractor/guidance/turn_classification.hpp"
#include "extractor/guidance/toolkit.hpp"
#include "extractor/restriction_map.hpp"
#include "extractor/compressed_edge_container.hpp"

#include <cstdint>

#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

namespace osrm
{
namespace extractor
{
namespace guidance
{

struct TurnCandidate
{
    EdgeID eid;   // the id of the arc
    bool valid;   // a turn may be relevant to good instructions, even if we cannot take the road
    double angle; // the approximated angle of the turn
    TurnInstruction instruction; // a proposed instruction
    double confidence;           // how close to the border is the turn?

    std::string toString() const
    {
        std::string result = "[turn] ";
        result += std::to_string(eid);
        result += " valid: ";
        result += std::to_string(valid);
        result += " angle: ";
        result += std::to_string(angle);
        result += " instruction: ";
        result += std::to_string(static_cast<std::int32_t>(instruction.type)) + " " +
                  std::to_string(static_cast<std::int32_t>(instruction.direction_modifier));
        result += " confidence: ";
        result += std::to_string(confidence);
        return result;
    }
};

// the entry into the turn analysis
std::vector<TurnCandidate> getTurns(const NodeID from_node,
                                    const EdgeID via_eid,
                                    const util::NodeBasedDynamicGraph &node_based_graph,
                                    const std::vector<QueryNode> &node_info_list,
                                    const RestrictionMap &restriction_map,
                                    const std::unordered_set<NodeID> &barrier_nodes,
                                    const CompressedEdgeContainer &compressed_edge_container);

namespace detail
{

// Check for restrictions/barriers and generate a list of valid and invalid turns present at the
// node reached
// from `from_node` via `via_eid`
// The resulting candidates have to be analysed for their actual instructions later on.
std::vector<TurnCandidate>
getTurnCandidates(const NodeID from_node,
                  const EdgeID via_eid,
                  const util::NodeBasedDynamicGraph &node_based_graph,
                  const std::vector<QueryNode> &node_info_list,
                  const RestrictionMap &restriction_map,
                  const std::unordered_set<NodeID> &barrier_nodes,
                  const CompressedEdgeContainer &compressed_edge_container);

// Merge segregated roads to omit invalid turns in favor of treating segregated roads as one.
// This function combines roads the following way:
//
//     *                           *
//     *        is converted to    *
//   v   ^                         +
//   v   ^                         +
//
// The treatment results in a straight turn angle of 180º rather than a turn angle of approx 160
std::vector<TurnCandidate>
mergeSegregatedRoads(const NodeID from_node,
                     const EdgeID via_eid,
                     std::vector<TurnCandidate> turn_candidates,
                     const util::NodeBasedDynamicGraph &node_based_graph);

// TODO distinguish roundabouts and rotaries
// TODO handle bike/walk cases that allow crossing a roundabout!

// Processing of roundabouts
// Produces instructions to enter/exit a roundabout or to stay on it.
// Performs the distinction between roundabout and rotaries.
std::vector<TurnCandidate> handleRoundabouts(const NodeID from,
                                             const EdgeID via_edge,
                                             const bool on_roundabout,
                                             const bool can_enter_roundabout,
                                             const bool can_exit_roundabout,
                                             std::vector<TurnCandidate> turn_candidates,
                                             const util::NodeBasedDynamicGraph &node_based_graph);

// Indicates a Junction containing a motoryway
bool isMotorwayJunction(const NodeID from,
                        const EdgeID via_edge,
                        const std::vector<TurnCandidate> &turn_candidates,
                        const util::NodeBasedDynamicGraph &node_based_graph);

// Decide whether a turn is a turn or a ramp access
TurnType findBasicTurnType(const NodeID from,
                           const EdgeID via_edge,
                           const TurnCandidate &candidate,
                           const util::NodeBasedDynamicGraph &node_based_graph);

// Get the Instruction for an obvious turn
// Instruction will be a silent instruction
TurnInstruction getInstructionForObvious(const std::size_t number_of_candidates,
                                         const NodeID from,
                                         const EdgeID via_edge,
                                         const TurnCandidate &candidate,
                                         const util::NodeBasedDynamicGraph &node_based_graph);

// Helper Function that decides between NoTurn or NewName
TurnInstruction noTurnOrNewName(const NodeID from,
                                const EdgeID via_edge,
                                const TurnCandidate &candidate,
                                const util::NodeBasedDynamicGraph &node_based_graph);

// Basic Turn Handling

// Dead end.
std::vector<TurnCandidate> handleOneWayTurn(const NodeID from,
                                            const EdgeID via_edge,
                                            std::vector<TurnCandidate> turn_candidates,
                                            const util::NodeBasedDynamicGraph &node_based_graph);

// Mode Changes, new names...
std::vector<TurnCandidate> handleTwoWayTurn(const NodeID from,
                                            const EdgeID via_edge,
                                            std::vector<TurnCandidate> turn_candidates,
                                            const util::NodeBasedDynamicGraph &node_based_graph);

// Forks, T intersections and similar
std::vector<TurnCandidate> handleThreeWayTurn(const NodeID from,
                                              const EdgeID via_edge,
                                              std::vector<TurnCandidate> turn_candidates,
                                              const util::NodeBasedDynamicGraph &node_based_graph);

// Normal Intersection. Can still contain forks...
std::vector<TurnCandidate> handleFourWayTurn(const NodeID from,
                                             const EdgeID via_edge,
                                             std::vector<TurnCandidate> turn_candidates,
                                             const util::NodeBasedDynamicGraph &node_based_graph);

// Fallback for turns of high complexion
std::vector<TurnCandidate> handleComplexTurn(const NodeID from,
                                             const EdgeID via_edge,
                                             std::vector<TurnCandidate> turn_candidates,
                                             const util::NodeBasedDynamicGraph &node_based_graph);

// Any Junction containing motorways
std::vector<TurnCandidate>
handleMotorwayJunction(const NodeID from,
                       const EdgeID via_edge,
                       std::vector<TurnCandidate> turn_candidates,
                       const util::NodeBasedDynamicGraph &node_based_graph);

// Utility function, setting basic turn types. Prepares for normal turn handling.
std::vector<TurnCandidate> setTurnTypes(const NodeID from,
                                        const EdgeID via_edge,
                                        std::vector<TurnCandidate> turn_candidates,
                                        const util::NodeBasedDynamicGraph &node_based_graph);

// Utility function to handle direction modifier conflicts if reasonably possible
std::vector<TurnCandidate> handleConflicts(const NodeID from,
                                           const EdgeID via_edge,
                                           std::vector<TurnCandidate> turn_candidates,
                                           const util::NodeBasedDynamicGraph &node_based_graph);

// Old fallbacks, to be removed
std::vector<TurnCandidate> optimizeRamps(const EdgeID via_edge,
                                         std::vector<TurnCandidate> turn_candidates,
                                         const util::NodeBasedDynamicGraph &node_based_graph);

std::vector<TurnCandidate> optimizeCandidates(const EdgeID via_eid,
                                              std::vector<TurnCandidate> turn_candidates,
                                              const util::NodeBasedDynamicGraph &node_based_graph,
                                              const std::vector<QueryNode> &node_info_list);

bool isObviousChoice(const EdgeID via_eid,
                     const std::size_t turn_index,
                     const std::vector<TurnCandidate> &turn_candidates,
                     const util::NodeBasedDynamicGraph &node_based_graph);

std::vector<TurnCandidate> suppressTurns(const EdgeID via_eid,
                                         std::vector<TurnCandidate> turn_candidates,
                                         const util::NodeBasedDynamicGraph &node_based_graph);

// node_u -- (edge_1) --> node_v -- (edge_2) --> node_w
TurnInstruction AnalyzeTurn(const NodeID node_u,
                            const EdgeID edge1,
                            const NodeID node_v,
                            const EdgeID edge2,
                            const NodeID node_w,
                            const double angle,
                            const util::NodeBasedDynamicGraph &node_based_graph);

// Assignment of specific turn types
void assignFork(const EdgeID via_edge,
                TurnCandidate &left,
                TurnCandidate &right,
                const util::NodeBasedDynamicGraph &node_based_graph);
void assignFork(const EdgeID via_edge,
                TurnCandidate &left,
                TurnCandidate &center,
                TurnCandidate &right,
                const util::NodeBasedDynamicGraph &node_based_graph);

} // namespace detail
} // namespace guidance
} // namespace extractor
} // namespace osrm

#endif // OSRM_EXTRACTOR_TURN_ANALYSIS

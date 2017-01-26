#include "partition/inertial_flow.hpp"
#include "partition/bisection_graph.hpp"
#include "partition/reorder_first_last.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <mutex>
#include <set>
#include <tuple>
#include <utility>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

namespace osrm
{
namespace partition
{

InertialFlow::InertialFlow(const GraphView &view_) : view(view_) {}

std::vector<bool> InertialFlow::ComputePartition(const double balance,
                                                 const double source_sink_rate)
{
    auto cut = BestMinCut(10 /* should be taken from outside */, source_sink_rate);

    std::cout << "Partition: ";
    for (auto b : cut.flags)
        std::cout << b;
    std::cout << std::endl;

    return cut.flags;
}

InertialFlow::SpatialOrder InertialFlow::MakeSpatialOrder(const double ratio,
                                                          const double slope) const
{
    struct NodeWithCoordinate
    {
        NodeWithCoordinate(NodeID nid_, util::Coordinate coordinate_)
            : nid{nid_}, coordinate{std::move(coordinate_)}
        {
        }

        NodeID nid;
        util::Coordinate coordinate;
    };

    using Embedding = std::vector<NodeWithCoordinate>;

    Embedding embedding;
    embedding.reserve(view.NumberOfNodes());

    std::transform(view.Begin(), view.End(), std::back_inserter(embedding), [&](const auto nid) {
        return NodeWithCoordinate{nid, view.GetNode(nid).coordinate};
    });

    const auto project = [slope](const auto &each) {
        auto lon = static_cast<std::int32_t>(each.coordinate.lon);
        auto lat = static_cast<std::int32_t>(each.coordinate.lat);

        return slope * lon + (1. - std::fabs(slope)) * lat;
    };

    const auto spatially = [&](const auto &lhs, const auto &rhs) {
        return project(lhs) < project(rhs);
    };

    const std::size_t n = ratio * embedding.size();

    reorderFirstLast(embedding, n, spatially);

    InertialFlow::SpatialOrder order;

    order.sources.reserve(n);
    order.sinks.reserve(n);

    for (auto it = begin(embedding), last = begin(embedding) + n; it != last; ++it)
        order.sources.insert(it->nid);

    for (auto it = end(embedding) - n, last = end(embedding); it != last; ++it)
        order.sinks.insert(it->nid);

    return order;
}

DinicMaxFlow::MinCut InertialFlow::BestMinCut(const std::size_t n, const double ratio) const
{
    auto base = MakeSpatialOrder(ratio, -1.);
    auto best = DinicMaxFlow()(view, base.sources, base.sinks);

    const auto get_balance = [this](const auto num_nodes_source) {
        double ratio = static_cast<double>(view.NumberOfNodes() - num_nodes_source) /
                       static_cast<double>(num_nodes_source);
        return std::abs(ratio - 1.0);
    };

    auto best_balance = get_balance(best.num_nodes_source);

    std::mutex lock;

    tbb::blocked_range<std::size_t> range{1, n + 1};

    tbb::parallel_for(range, [&, this](const auto &chunk) {
        for (auto round = chunk.begin(), end = chunk.end(); round != end; ++round)
        {
            const auto slope = -1. + round * (2. / n);

            auto order = this->MakeSpatialOrder(ratio, slope);
            auto cut = DinicMaxFlow()(view, order.sources, order.sinks);
            auto cut_balance = get_balance(cut.num_nodes_source);

            {
                std::lock_guard<std::mutex> guard{lock};

                // Swap to keep the destruction of the old object outside of critical section.
                if (std::tie(cut.num_edges, cut_balance) < std::tie(best.num_edges, best_balance))
                {
                    best_balance = cut_balance;
                    std::swap(best, cut);
                }
            }

            // cut gets destroyed here
        }
    });

    return best;
}

} // namespace partition
} // namespace osrm

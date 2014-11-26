/*

Copyright (c) 2013, Project OSRM, Dennis Luxen, others
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef STATIC_GRAPH_H
#define STATIC_GRAPH_H

#include "Percent.h"
#include "Range.h"
#include "SharedMemoryVectorWrapper.h"
#include "RangeTable.h"
#include "../typedefs.h"

#include <boost/assert.hpp>

#include <tbb/parallel_sort.h>

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>


template <typename EdgeDataT, bool UseSharedMemory = false> class StaticGraph
{
  public:
    using NodeIterator = NodeID;
    using EdgeIterator = NodeID;
    using EdgeData = EdgeDataT;
    using EdgeRange = osrm::range<EdgeIterator>;

    class InputEdge
    {
      public:
        NodeIterator source;
        NodeIterator target;
        EdgeDataT data;

        template<typename... Ts>
        InputEdge(NodeIterator source, NodeIterator target, Ts &&...data) : source(source), target(target), data(std::forward<Ts>(data)...) { }
        bool operator<(const InputEdge &right) const
        {
            if (source != right.source)
            {
                return source < right.source;
            }
            return target < right.target;
        }
    };

    struct EdgeArrayEntry
    {
        NodeID target;
        EdgeDataT data;
    };

    using NodeTable = RangeTable<16, UseSharedMemory>;

    template<typename VectorT>
    static void AdjacencyArrayFromEdges(const unsigned num_nodes, const VectorT &edges,
                                 NodeTable &node_table,
                                 std::vector<EdgeArrayEntry>& edge_array)
    {
        std::vector<EdgeIterator> node_degree_array(num_nodes);
        edge_array.resize(edges.size());
        EdgeIterator edge_id = 0;
        for (const auto node_id : osrm::irange(0u, num_nodes))
        {
            EdgeIterator num_neightbours = 0;
            while (edge_id < edges.size() && edges[edge_id].source == node_id)
            {
                ++num_neightbours;
                ++edge_id;
            }
            node_degree_array[node_id] = num_neightbours;
        }
        edge_array.resize(edges.size());
        edge_id = 0;
        for (const auto num_neightbours : node_degree_array)
        {
            EdgeIterator end_interval = edge_id + num_neightbours;
            while (edge_id < end_interval)
            {
                edge_array[edge_id].target = edges[edge_id].target;
                edge_array[edge_id].data = edges[edge_id].data;
                BOOST_ASSERT(edge_array[edge].data.distance > 0);
                edge_id++;
            }
        }

        node_table = NodeTable(node_degree_array);
    }

    EdgeRange GetAdjacentEdgeRange(const NodeID node) const
    {
        return node_idx.GetRange(node);
    }


    StaticGraph(RangeTable<16, UseSharedMemory> &nodes,
                typename ShM<EdgeArrayEntry, UseSharedMemory>::vector &edges)
    {
        number_of_nodes = static_cast<decltype(number_of_nodes)>(node_idx.GetSum());
        number_of_edges = static_cast<decltype(number_of_edges)>(edges.size());

        node_idx.swap(nodes);
        edge_array.swap(edges);
    }

    unsigned GetNumberOfNodes() const { return number_of_nodes; }

    unsigned GetNumberOfEdges() const { return number_of_edges; }

    unsigned GetOutDegree(const NodeIterator n) const
    {
        osrm::range<unsigned> range = node_idx.GetRange(n);
        if (range.begin() != range.end())
        {
            return range.back() - range.front() + 1;
        }
        return 0;
    }

    inline NodeIterator GetTarget(const EdgeIterator e) const
    {
        return NodeIterator(edge_array[e].target);
    }

    inline EdgeDataT &GetEdgeData(const EdgeIterator e) { return edge_array[e].data; }

    const EdgeDataT &GetEdgeData(const EdgeIterator e) const { return edge_array[e].data; }

    EdgeIterator BeginEdges(const NodeIterator n) const
    {
        return node_idx.GetRange(n).front();
    }

    EdgeIterator EndEdges(const NodeIterator n) const
    {
        return node_idx.GetRange(n).back();
    }

    // searches for a specific edge
    EdgeIterator FindEdge(const NodeIterator from, const NodeIterator to) const
    {
        EdgeIterator smallest_edge = SPECIAL_EDGEID;
        EdgeWeight smallest_weight = INVALID_EDGE_WEIGHT;
        for (auto edge : GetAdjacentEdgeRange(from))
        {
            const NodeID target = GetTarget(edge);
            const EdgeWeight weight = GetEdgeData(edge).distance;
            if (target == to && weight < smallest_weight)
            {
                smallest_edge = edge;
                smallest_weight = weight;
            }
        }
        return smallest_edge;
    }

    EdgeIterator FindEdgeInEitherDirection(const NodeIterator from, const NodeIterator to) const
    {
        EdgeIterator tmp = FindEdge(from, to);
        return (SPECIAL_NODEID != tmp ? tmp : FindEdge(to, from));
    }

    EdgeIterator
    FindEdgeIndicateIfReverse(const NodeIterator from, const NodeIterator to, bool &result) const
    {
        EdgeIterator current_iterator = FindEdge(from, to);
        if (SPECIAL_NODEID == current_iterator)
        {
            current_iterator = FindEdge(to, from);
            if (SPECIAL_NODEID != current_iterator)
            {
                result = true;
            }
        }
        return current_iterator;
    }

  private:
    NodeIterator number_of_nodes;
    EdgeIterator number_of_edges;

    RangeTable<16, UseSharedMemory> node_idx;
    typename ShM<EdgeArrayEntry, UseSharedMemory>::vector edge_array;
};

#endif // STATIC_GRAPH_H

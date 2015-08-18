/*

Copyright (c) 2015, Project OSRM contributors
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

#ifndef ROUND_TRIP_HPP
#define ROUND_TRIP_HPP

#include "plugin_base.hpp"

#include "../algorithms/object_encoder.hpp"
#include "../algorithms/tiny_components.hpp"
#include "../routing_algorithms/tsp_nearest_neighbour.hpp"
#include "../routing_algorithms/tsp_farthest_insertion.hpp"
#include "../routing_algorithms/tsp_brute_force.hpp"
#include "../routing_algorithms/tsp_jarnik_prim.hpp"
#include "../data_structures/query_edge.hpp"
#include "../data_structures/search_engine.hpp"
#include "../data_structures/matrix_graph_wrapper.hpp"
#include "../descriptors/descriptor_base.hpp"
#include "../descriptors/json_descriptor.hpp"
#include "../util/json_renderer.hpp"
#include "../util/make_unique.hpp"
#include "../util/string_util.hpp"
#include "../util/timing_util.hpp"
#include "../util/simple_logger.hpp"

#include <osrm/json_container.hpp>

#include <cstdlib>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <limits>

#include <iostream>

template <class DataFacadeT> class RoundTripPlugin final : public BasePlugin
{
  private:
    std::string descriptor_string;
    DataFacadeT *facade;
    std::unique_ptr<SearchEngine<DataFacadeT>> search_engine_ptr;

  public:
    explicit RoundTripPlugin(DataFacadeT *facade)
        : descriptor_string("trip"), facade(facade)
    {
        search_engine_ptr = osrm::make_unique<SearchEngine<DataFacadeT>>(facade);
    }

    const std::string GetDescriptor() const override final { return descriptor_string; }

    void GetPhantomNodes(const RouteParameters &route_parameters, PhantomNodeArray & phantom_node_vector) {
        const bool checksum_OK = (route_parameters.check_sum == facade->GetCheckSum());

        // find phantom nodes for all input coords
        for (const auto i : osrm::irange<std::size_t>(0, route_parameters.coordinates.size())) {
            // if client hints are helpful, encode hints
            if (checksum_OK && i < route_parameters.hints.size() &&
                !route_parameters.hints[i].empty()) {
                PhantomNode current_phantom_node;
                ObjectEncoder::DecodeFromBase64(route_parameters.hints[i], current_phantom_node);
                if (current_phantom_node.is_valid(facade->GetNumberOfNodes()))
                {
                    phantom_node_vector[i].emplace_back(std::move(current_phantom_node));
                    continue;
                }
            }
            facade->IncrementalFindPhantomNodeForCoordinate(route_parameters.coordinates[i],
                                                            phantom_node_vector[i], 1);
            if (phantom_node_vector[i].size() > 1) {
                phantom_node_vector[i].erase(phantom_node_vector[i].begin());
            }
            BOOST_ASSERT(phantom_node_vector[i].front().is_valid(facade->GetNumberOfNodes()));
        }
    }

    void SplitUnaccessibleLocations(PhantomNodeArray & phantom_node_vector,
                                    std::vector<EdgeWeight> & result_table,
                                    std::vector<std::vector<unsigned>> & components) {

        // Run TarjanSCC
        auto number_of_locations = phantom_node_vector.size();
        auto wrapper = std::make_shared<MatrixGraphWrapper<EdgeWeight>>(result_table, number_of_locations);
        auto empty_restriction = RestrictionMap(std::vector<TurnRestriction>());
        auto empty_vector = std::vector<bool>();
        auto scc = TarjanSCC<MatrixGraphWrapper<EdgeWeight>>(wrapper, empty_restriction, empty_vector);
        scc.run();

        for (int j = 0; j < scc.get_number_of_components(); ++j){
            components.push_back(std::vector<unsigned>());
        }

        for (int i = 0; i < number_of_locations; ++i) {
            components[scc.get_component_id(i)].push_back(i);
        }
    }

    template <typename number>
    void SetLocPermutationOutput(const std::vector<number> & loc_permutation, osrm::json::Object & json_result){
        osrm::json::Array json_loc_permutation;
        json_loc_permutation.values.insert(json_loc_permutation.values.end(), loc_permutation.begin(), loc_permutation.end());
        json_result.values["loc_permutation"] = json_loc_permutation;
    }

    void SetDistanceOutput(const int distance, osrm::json::Object & json_result) {
        json_result.values["distance"] = distance;
    }

    void SetRuntimeOutput(const float runtime, osrm::json::Object & json_result) {
        json_result.values["runtime"] = runtime;
    }

    void SetGeometry(const RouteParameters &route_parameters, const InternalRouteResult & min_route, osrm::json::Object & json_result) {
        // return geometry result to json
        std::unique_ptr<BaseDescriptor<DataFacadeT>> descriptor;
        descriptor = osrm::make_unique<JSONDescriptor<DataFacadeT>>(facade);

        descriptor->SetConfig(route_parameters);
        descriptor->Run(min_route, json_result);
    }

    void ComputeRoute(const PhantomNodeArray & phantom_node_vector,
                      const RouteParameters & route_parameters,
                      std::vector<unsigned> & trip,
                      InternalRouteResult & min_route) {
        // given he final trip, compute total distance and return the route and location permutation
        PhantomNodes viapoint;
        for (auto it = trip.begin(); it != std::prev(trip.end()); ++it) {
            auto from_node = *it;
            auto to_node = *std::next(it);
            viapoint = PhantomNodes{phantom_node_vector[from_node][0], phantom_node_vector[to_node][0]};
            min_route.segment_end_coordinates.emplace_back(viapoint);
        }
        // check dist between last and first location too
        viapoint = PhantomNodes{phantom_node_vector[*std::prev(trip.end())][0], phantom_node_vector[trip.front()][0]};
        min_route.segment_end_coordinates.emplace_back(viapoint);
        search_engine_ptr->shortest_path(min_route.segment_end_coordinates, route_parameters.uturns, min_route);
    }

    void ComputeRoute(const PhantomNodeArray & phantom_node_vector,
                      const RouteParameters & route_parameters,
                      std::vector<std::vector<unsigned>> & trip,
                      std::vector<InternalRouteResult> & route) {
        for (auto curr_trip : trip) {
            InternalRouteResult curr_route;
            ComputeRoute(phantom_node_vector, route_parameters, curr_trip, curr_route);
            route.push_back(curr_route);
            search_engine_ptr->shortest_path(route.back().segment_end_coordinates, route_parameters.uturns, route.back());
        }
    }

    int HandleRequest(const RouteParameters &route_parameters,
                      osrm::json::Object &json_result) override final
    {
        // check if all inputs are coordinates
        if (!check_all_coordinates(route_parameters.coordinates)) {
            return 400;
        }

        PhantomNodeArray phantom_node_vector(route_parameters.coordinates.size());
        GetPhantomNodes(route_parameters, phantom_node_vector);

        // compute the distance table of all phantom nodes
        const std::shared_ptr<std::vector<EdgeWeight>> result_table =
            search_engine_ptr->distance_table(phantom_node_vector);
        if (!result_table){
            return 400;
        }

        //check if locations are in different strongly connected components (SCC)
        const auto maxint = std::numeric_limits<int>::max();
        if (*std::max_element(result_table->begin(), result_table->end()) == maxint) {

            // JSON output related objects
            std::unique_ptr<BaseDescriptor<DataFacadeT>> descriptor;
            descriptor = osrm::make_unique<JSONDescriptor<DataFacadeT>>(facade);
            descriptor->SetConfig(route_parameters);

            TIMER_START(tsp);
            // Compute all SCC
            std::vector<std::vector<unsigned>> components;
            SplitUnaccessibleLocations(phantom_node_vector, *result_table, components);
            // std::vector<std::vector<unsigned>> res_route (components.size()-1);
            std::vector<std::vector<unsigned>> res_route;

            //run TSP computation for every SCC
            for(auto k = 0; k < components.size(); ++k) {
                if (components[k].size() > 1) {
                    std::vector<unsigned> scc_route;
                    scc_route.reserve(components[k].size());

                    // Compute the TSP with the given algorithm
                    if (route_parameters.tsp_algo == "BF" && route_parameters.coordinates.size() < 10) {
                        SimpleLogger().Write() << "Running SCC BF";
                        osrm::tsp::BruteForceTSP(components[k], phantom_node_vector, *result_table, scc_route);
                        res_route.push_back(scc_route);
                    } else if (route_parameters.tsp_algo == "NN") {
                        SimpleLogger().Write() << "Running SCC NN";
                        osrm::tsp::NearestNeighbourTSP(components[k], phantom_node_vector, *result_table, scc_route);
                        res_route.push_back(scc_route);
                    } else if (route_parameters.tsp_algo == "FI") {
                        SimpleLogger().Write() << "Running SCC FI";
                        osrm::tsp::FarthestInsertionTSP(components[k], phantom_node_vector, *result_table, scc_route);
                        res_route.push_back(scc_route);
                    } else{
                        SimpleLogger().Write() << "Running SCC FI";
                        osrm::tsp::FarthestInsertionTSP(components[k], phantom_node_vector, *result_table, scc_route);
                        res_route.push_back(scc_route);
                    }
                }
            }
            SimpleLogger().Write() << "DONE";
            std::vector<InternalRouteResult> route;
            ComputeRoute(phantom_node_vector, route_parameters, res_route, route);
            TIMER_STOP(tsp);
            SetRuntimeOutput(TIMER_MSEC(tsp), json_result);

            // SimpleLogger().Write() << "Route is";
            // for (auto x : res_route) {
            //     for (auto y : x)
            //         std::cout << y << " ";
            // }
            // SimpleLogger().Write() << "";

            auto dist = 0;
            for (auto curr_route : route) {
                dist += curr_route.shortest_path_length;
                SetGeometry(route_parameters, curr_route, json_result);
            }
            SetDistanceOutput(dist, json_result);
        } else { //run TSP computation for all locations
            auto number_of_locations = phantom_node_vector.size();
            std::vector<unsigned> res_route;
            res_route.reserve(number_of_locations);
            InternalRouteResult min_route;

            // Compute the TSP with the given algorithm
            TIMER_START(tsp);
            if (route_parameters.tsp_algo == "BF" && route_parameters.coordinates.size() < 10) {
                SimpleLogger().Write() << "Running BF";
                osrm::tsp::BruteForceTSP(phantom_node_vector, *result_table, res_route);
            } else if (route_parameters.tsp_algo == "NN") {
                SimpleLogger().Write() << "Running NN";
                osrm::tsp::NearestNeighbourTSP(phantom_node_vector, *result_table, res_route);
            } else if (route_parameters.tsp_algo == "FI") {
                SimpleLogger().Write() << "Running FI";
                osrm::tsp::FarthestInsertionTSP(phantom_node_vector, *result_table, res_route);
            } else {
                SimpleLogger().Write() << "Running FI";
                osrm::tsp::FarthestInsertionTSP(phantom_node_vector, *result_table, res_route);
                // osrm::tsp::NearestNeighbourTSP(phantom_node_vector, *result_table, res_route);
            }
            SimpleLogger().Write() << "DONE";
            ComputeRoute(phantom_node_vector, route_parameters, res_route, min_route);
            TIMER_STOP(tsp);

            // SimpleLogger().Write() << "Route is";
            // for (auto x : res_route) {
            //     std::cout << x << " ";
            // }
            // SimpleLogger().Write() << "";

            SetRuntimeOutput(TIMER_MSEC(tsp), json_result);
            SetLocPermutationOutput(res_route, json_result);
            SetDistanceOutput(min_route.shortest_path_length, json_result);
            SetGeometry(route_parameters, min_route, json_result);
            BOOST_ASSERT(min_route.segment_end_coordinates.size() == route_parameters.coordinates.size());
        }

        return 200;
    }

};

#endif // ROUND_TRIP_HPP

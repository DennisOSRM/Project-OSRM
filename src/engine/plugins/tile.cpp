#include "engine/plugins/plugin_base.hpp"
#include "engine/plugins/tile.hpp"

#include <protozero/varint.hpp>
#include <protozero/pbf_writer.hpp>

#include <string>
#include <vector>
#include <utility>

#include <cmath>
#include <cstdint>

namespace osrm
{
namespace engine
{
namespace plugins
{

// from mapnik/well_known_srs.hpp
const constexpr double EARTH_RADIUS = 6378137.0;
const constexpr double EARTH_DIAMETER = EARTH_RADIUS * 2.0;
const constexpr double EARTH_CIRCUMFERENCE = EARTH_DIAMETER * M_PI;
const constexpr double MAXEXTENT = EARTH_CIRCUMFERENCE / 2.0;
const constexpr double M_PI_by2 = M_PI / 2.0;
const constexpr double D2R = M_PI / 180.0;
const constexpr double R2D = 180.0 / M_PI;
const constexpr double M_PIby360 = M_PI / 360.0;
const constexpr double MAXEXTENTby180 = MAXEXTENT / 180.0;
const double MAX_LATITUDE = R2D * (2.0 * std::atan(std::exp(180.0 * D2R)) - M_PI_by2);
// ^ math functions are not constexpr since they have side-effects (setting errno) :(

// from mapnik-vector-tile
namespace detail_pbf
{

inline unsigned encode_length(const unsigned len) { return (len << 3u) | 2u; }
}

// Converts a regular WSG84 lon/lat pair into
// a mercator coordinate
inline void lonlat2merc(double &x, double &y)
{
    if (x > 180)
        x = 180;
    else if (x < -180)
        x = -180;
    if (y > MAX_LATITUDE)
        y = MAX_LATITUDE;
    else if (y < -MAX_LATITUDE)
        y = -MAX_LATITUDE;
    x = x * MAXEXTENTby180;
    y = std::log(std::tan((90 + y) * M_PIby360)) * R2D;
    y = y * MAXEXTENTby180;
}

// This is the global default tile size for all Mapbox Vector Tiles
const constexpr double tile_size_ = 256.0;

//
inline void from_pixels(const double shift, double &x, double &y)
{
    const double b = shift / 2.0;
    x = (x - b) / (shift / 360.0);
    const double g = (y - b) / -(shift / (2 * M_PI));
    y = R2D * (2.0 * std::atan(std::exp(g)) - M_PI_by2);
}

// Converts a WMS tile coordinate (z,x,y) into a mercator bounding box
inline void xyz2mercator(
    const int x, const int y, const int z, double &minx, double &miny, double &maxx, double &maxy)
{
    minx = x * tile_size_;
    miny = (y + 1.0) * tile_size_;
    maxx = (x + 1.0) * tile_size_;
    maxy = y * tile_size_;
    const double shift = std::pow(2.0, z) * tile_size_;
    from_pixels(shift, minx, miny);
    from_pixels(shift, maxx, maxy);
    lonlat2merc(minx, miny);
    lonlat2merc(maxx, maxy);
}

// Converts a WMS tile coordinate (z,x,y) into a wsg84 bounding box
inline void xyz2wsg84(
    const int x, const int y, const int z, double &minx, double &miny, double &maxx, double &maxy)
{
    minx = x * tile_size_;
    miny = (y + 1.0) * tile_size_;
    maxx = (x + 1.0) * tile_size_;
    maxy = y * tile_size_;
    const double shift = std::pow(2.0, z) * tile_size_;
    from_pixels(shift, minx, miny);
    from_pixels(shift, maxx, maxy);
}

// emulates mapbox::box2d, just a simple container for
// a box
struct bbox final
{
    bbox(const double _minx, const double _miny, const double _maxx, const double _maxy)
        : minx(_minx), miny(_miny), maxx(_maxx), maxy(_maxy)
    {
    }

    double width() const { return maxx - minx; }
    double height() const { return maxy - miny; }

    const double minx;
    const double miny;
    const double maxx;
    const double maxy;
};

// Simple container class for WSG84 coordinates
struct point_type_d final
{
    point_type_d(double _x, double _y) : x(_x), y(_y) {}

    const double x;
    const double y;
};

// Simple container for integer coordinates (i.e. pixel coords)
struct point_type_i final
{
    point_type_i(std::int64_t _x, std::int64_t _y) : x(_x), y(_y) {}

    const std::int64_t x;
    const std::int64_t y;
};

using line_type = std::vector<point_type_i>;
using line_typed = std::vector<point_type_d>;

// from mapnik-vector-tile
// Encodes a linestring using protobuf zigzag encoding
inline bool encode_linestring(line_type line,
                              protozero::packed_field_uint32 &geometry,
                              std::int32_t &start_x,
                              std::int32_t &start_y)
{
    const std::size_t line_size = line.size();
    if (line_size < 2)
    {
        return false;
    }

    const unsigned line_to_length = static_cast<const unsigned>(line_size) - 1;

    auto pt = line.begin();
    geometry.add_element(9); // move_to | (1 << 3)
    geometry.add_element(protozero::encode_zigzag32(pt->x - start_x));
    geometry.add_element(protozero::encode_zigzag32(pt->y - start_y));
    start_x = pt->x;
    start_y = pt->y;
    geometry.add_element(detail_pbf::encode_length(line_to_length));
    for (++pt; pt != line.end(); ++pt)
    {
        const std::int32_t dx = pt->x - start_x;
        const std::int32_t dy = pt->y - start_y;
        geometry.add_element(protozero::encode_zigzag32(dx));
        geometry.add_element(protozero::encode_zigzag32(dy));
        start_x = pt->x;
        start_y = pt->y;
    }
    return true;
}

Status TilePlugin::HandleRequest(const api::TileParameters &parameters, std::string &pbf_buffer)
{

    // Vector tiles are 4096 virtual pixels on each side
    const double tile_extent = 4096.0;
    double min_lon, min_lat, max_lon, max_lat;

    // Convert the z,x,y mercator tile coordinates into WSG84 lon/lat values
    xyz2wsg84(parameters.x, parameters.y, parameters.z, min_lon, min_lat, max_lon, max_lat);

    util::Coordinate southwest{util::FloatLongitude(min_lon), util::FloatLatitude(min_lat)};
    util::Coordinate northeast{util::FloatLongitude(max_lon), util::FloatLatitude(max_lat)};

    // Fetch all the segments that are in our bounding box.
    // This hits the OSRM StaticRTree
    const auto edges = facade.GetEdgesInBox(southwest, northeast);

    // TODO: extract speed values for compressed and uncompressed geometries

    // Convert tile coordinates into mercator coordinates
    xyz2mercator(parameters.x, parameters.y, parameters.z, min_lon, min_lat, max_lon, max_lat);
    const bbox tile_bbox{min_lon, min_lat, max_lon, max_lat};

    // Protobuf serialized blocks when objects go out of scope, hence
    // the extra scoping below.
    protozero::pbf_writer tile_writer{pbf_buffer};
    {
        // Add a layer object to the PBF stream.  3=='layer' from the vector tile spec (2.1)
        protozero::pbf_writer layer_writer(tile_writer, 3);
        // TODO: don't write a layer if there are no features

        layer_writer.add_uint32(15, 2); // version
        // Field 1 is the "layer name" field, it's a string
        layer_writer.add_string(1, "speeds"); // name
        // Field 5 is the tile extent.  It's a uint32 and should be set to 4096
        // for normal vector tiles.
        layer_writer.add_uint32(5, 4096); // extent

        // Begin the layer features block
        {
            // Each feature gets a unique id, starting at 1
            unsigned id = 1;
            for (const auto &edge : edges)
            {
                // Get coordinates for start/end nodes of segmet (NodeIDs u and v)
                const auto a = facade.GetCoordinateOfNode(edge.u);
                const auto b = facade.GetCoordinateOfNode(edge.v);
                // Calculate the length in meters
                const double length = osrm::util::coordinate_calculation::haversineDistance(a, b);

                int forward_weight = 0;
                int reverse_weight = 0;

                if (edge.forward_packed_geometry_id != SPECIAL_EDGEID)
                {
                    std::vector<EdgeWeight> forward_weight_vector;
                    facade.GetUncompressedWeights(edge.forward_packed_geometry_id,
                                                  forward_weight_vector);
                    forward_weight = forward_weight_vector[edge.fwd_segment_position];
                }

                if (edge.reverse_packed_geometry_id != SPECIAL_EDGEID)
                {
                    std::vector<EdgeWeight> reverse_weight_vector;
                    facade.GetUncompressedWeights(edge.reverse_packed_geometry_id,
                                                  reverse_weight_vector);

                    BOOST_ASSERT(edge.fwd_segment_position < reverse_weight_vector.size());

                    reverse_weight = reverse_weight_vector[reverse_weight_vector.size() -
                                                           edge.fwd_segment_position - 1];
                }

                // If this is a valid forward edge, go ahead and add it to the tile
                if (forward_weight != 0 && edge.forward_edge_based_node_id != SPECIAL_NODEID)
                {
                    std::int32_t start_x = 0;
                    std::int32_t start_y = 0;

                    line_typed geo_line;
                    geo_line.emplace_back(static_cast<double>(util::toFloating(a.lon)),
                                          static_cast<double>(util::toFloating(a.lat)));
                    geo_line.emplace_back(static_cast<double>(util::toFloating(b.lon)),
                                          static_cast<double>(util::toFloating(b.lat)));

                    // Calculate the speed for this line
                    std::uint32_t speed =
                        static_cast<std::uint32_t>(round(length / forward_weight * 10 * 3.6));

                    line_type tile_line;
                    for (auto const &pt : geo_line)
                    {
                        double px_merc = pt.x;
                        double py_merc = pt.y;
                        lonlat2merc(px_merc, py_merc);
                        // convert lon/lat to tile coordinates
                        const auto px = std::round(
                            ((px_merc - tile_bbox.minx) * tile_extent / 16.0 / tile_bbox.width()) *
                            tile_extent / 256.0);
                        const auto py = std::round(
                            ((tile_bbox.maxy - py_merc) * tile_extent / 16.0 / tile_bbox.height()) *
                            tile_extent / 256.0);
                        tile_line.emplace_back(px, py);
                    }

                    // Here, we save the two attributes for our feature: the speed and the
                    // is_small
                    // boolean.  We onl serve up speeds from 0-139, so all we do is save the
                    // first
                    protozero::pbf_writer feature_writer(layer_writer, 2);
                    // Field 3 is the "geometry type" field.  Value 2 is "line"
                    feature_writer.add_enum(3, 2); // geometry type
                    // Field 1 for the feature is the "id" field.
                    feature_writer.add_uint64(1, id++); // id
                    {
                        // When adding attributes to a feature, we have to write
                        // pairs of numbers.  The first value is the index in the
                        // keys array (written later), and the second value is the
                        // index into the "values" array (also written later).  We're
                        // not writing the actual speed or bool value here, we're saving
                        // an index into the "values" array.  This means many features
                        // can share the same value data, leading to smaller tiles.
                        protozero::packed_field_uint32 field(feature_writer, 2);

                        field.add_element(0); // "speed" tag key offset
                        field.add_element(
                            std::min(speed, 127u)); // save the speed value, capped at 127
                        field.add_element(1);       // "is_small" tag key offset
                        field.add_element(edge.component.is_tiny ? 0 : 1); // is_small feature
                    }
                    {
                        // Encode the geometry for the feature
                        protozero::packed_field_uint32 geometry(feature_writer, 4);
                        encode_linestring(tile_line, geometry, start_x, start_y);
                    }
                }

                // Repeat the above for the coordinates reversed and using the `reverse`
                // properties
                if (reverse_weight != 0 && edge.reverse_edge_based_node_id != SPECIAL_NODEID)
                {
                    std::int32_t start_x = 0;
                    std::int32_t start_y = 0;

                    line_typed geo_line;
                    geo_line.emplace_back(static_cast<double>(util::toFloating(b.lon)),
                                          static_cast<double>(util::toFloating(b.lat)));
                    geo_line.emplace_back(static_cast<double>(util::toFloating(a.lon)),
                                          static_cast<double>(util::toFloating(a.lat)));

                    const auto speed =
                        static_cast<const std::uint32_t>(round(length / reverse_weight * 10 * 3.6));

                    line_type tile_line;
                    for (auto const &pt : geo_line)
                    {
                        double px_merc = pt.x;
                        double py_merc = pt.y;
                        lonlat2merc(px_merc, py_merc);
                        // convert to integer tile coordinat
                        const auto px = std::round(
                            ((px_merc - tile_bbox.minx) * tile_extent / 16.0 / tile_bbox.width()) *
                            tile_extent / 256.0);
                        const auto py = std::round(
                            ((tile_bbox.maxy - py_merc) * tile_extent / 16.0 / tile_bbox.height()) *
                            tile_extent / 256.0);
                        tile_line.emplace_back(px, py);
                    }

                    protozero::pbf_writer feature_writer(layer_writer, 2);
                    feature_writer.add_enum(3, 2);      // geometry type
                    feature_writer.add_uint64(1, id++); // id
                    {
                        protozero::packed_field_uint32 field(feature_writer, 2);
                        field.add_element(0); // "speed" tag key offset
                        field.add_element(
                            std::min(speed, 127u)); // save the speed value, capped at 127
                        field.add_element(1);       // "is_small" tag key offset
                        field.add_element(edge.component.is_tiny ? 0 : 1); // is_small feature
                    }
                    {
                        protozero::packed_field_uint32 geometry(feature_writer, 4);
                        encode_linestring(tile_line, geometry, start_x, start_y);
                    }
                }
            }
        }

        // Field id 3 is the "keys" attribute
        // We need two "key" fields, these are referred to with 0 and 1 (their array indexes)
        // earlier
        layer_writer.add_string(3, "speed");
        layer_writer.add_string(3, "is_small");

        // Now, we write out the possible speed value arrays and possible is_tiny
        // values.  Field type 4 is the "values" field.  It's a variable type field,
        // so requires a two-step write (create the field, then write its value)
        for (std::size_t i = 0; i < 128; i++)
        {
            {
                // Writing field type 4 == variant type
                protozero::pbf_writer values_writer(layer_writer, 4);
                // Attribute value 5 == uin64 type
                values_writer.add_uint64(5, i);
            }
        }
        {
            protozero::pbf_writer values_writer(layer_writer, 4);
            // Attribute value 7 == bool type
            values_writer.add_bool(7, true);
        }
        {
            protozero::pbf_writer values_writer(layer_writer, 4);
            // Attribute value 7 == bool type
            values_writer.add_bool(7, false);
        }
    }

    return Status::Ok;
}
}
}
}

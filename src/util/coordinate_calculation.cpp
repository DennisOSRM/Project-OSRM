#include "util/coordinate_calculation.hpp"

#include "util/string_util.hpp"
#include "util/trigonometry_table.hpp"

#include <boost/assert.hpp>
#include <boost/math/constants/constants.hpp>

#include <cmath>

#include <limits>

namespace osrm
{
namespace util
{
namespace coordinate_calculation
{

double euclideanDistance(const Coordinate coordinate_1, const Coordinate coordinate_2)
{
    const double x1 = static_cast<double>(toFloating(coordinate_1.lon));
    const double y1 = mercator::latToY(toFloating(coordinate_1.lat));
    const double x2 = static_cast<double>(toFloating(coordinate_2.lon));
    const double y2 = mercator::latToY(toFloating(coordinate_2.lat));
    const double dx = x1 - x2;
    const double dy = y1 - y2;

    return std::sqrt(dx * dx + dy * dy);
}

double haversineDistance(const Coordinate coordinate_1, const Coordinate coordinate_2)
{
    auto lon1 = static_cast<int>(coordinate_1.lon);
    auto lat1 = static_cast<int>(coordinate_1.lat);
    auto lon2 = static_cast<int>(coordinate_2.lon);
    auto lat2 = static_cast<int>(coordinate_2.lat);
    BOOST_ASSERT(lon1 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lat1 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lon2 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lat2 != std::numeric_limits<int>::min());
    const double lt1 = lat1 / COORDINATE_PRECISION;
    const double ln1 = lon1 / COORDINATE_PRECISION;
    const double lt2 = lat2 / COORDINATE_PRECISION;
    const double ln2 = lon2 / COORDINATE_PRECISION;
    const double dlat1 = lt1 * (RAD);

    const double dlong1 = ln1 * (RAD);
    const double dlat2 = lt2 * (RAD);
    const double dlong2 = ln2 * (RAD);

    const double dlong = dlong1 - dlong2;
    const double dlat = dlat1 - dlat2;

    const double aharv = std::pow(std::sin(dlat / 2.0), 2.0) +
                         std::cos(dlat1) * std::cos(dlat2) * std::pow(std::sin(dlong / 2.), 2);
    const double charv = 2. * std::atan2(std::sqrt(aharv), std::sqrt(1.0 - aharv));
    return EARTH_RADIUS * charv;
}

double greatCircleDistance(const Coordinate coordinate_1, const Coordinate coordinate_2)
{
    auto lon1 = static_cast<int>(coordinate_1.lon);
    auto lat1 = static_cast<int>(coordinate_1.lat);
    auto lon2 = static_cast<int>(coordinate_2.lon);
    auto lat2 = static_cast<int>(coordinate_2.lat);
    BOOST_ASSERT(lat1 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lon1 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lat2 != std::numeric_limits<int>::min());
    BOOST_ASSERT(lon2 != std::numeric_limits<int>::min());

    const double float_lat1 = (lat1 / COORDINATE_PRECISION) * RAD;
    const double float_lon1 = (lon1 / COORDINATE_PRECISION) * RAD;
    const double float_lat2 = (lat2 / COORDINATE_PRECISION) * RAD;
    const double float_lon2 = (lon2 / COORDINATE_PRECISION) * RAD;

    const double x_value = (float_lon2 - float_lon1) * std::cos((float_lat1 + float_lat2) / 2.0);
    const double y_value = float_lat2 - float_lat1;
    return std::hypot(x_value, y_value) * EARTH_RADIUS;
}

double perpendicularDistance(const Coordinate source_coordinate,
                             const Coordinate target_coordinate,
                             const Coordinate query_location)
{
    double ratio;
    Coordinate nearest_location;

    return perpendicularDistance(source_coordinate, target_coordinate, query_location,
                                 nearest_location, ratio);
}

double perpendicularDistance(const Coordinate segment_source,
                             const Coordinate segment_target,
                             const Coordinate query_location,
                             Coordinate &nearest_location,
                             double &ratio)
{
    using namespace coordinate_calculation;

    return perpendicularDistanceFromProjectedCoordinate(
        segment_source, segment_target, query_location,
        {static_cast<double>(toFloating(query_location.lon)),
         mercator::latToY(toFloating(query_location.lat))},
        nearest_location, ratio);
}

double perpendicularDistanceFromProjectedCoordinate(
    const Coordinate source_coordinate,
    const Coordinate target_coordinate,
    const Coordinate query_location,
    const std::pair<double, double> projected_xy_coordinate)
{
    double ratio;
    Coordinate nearest_location;

    return perpendicularDistanceFromProjectedCoordinate(source_coordinate, target_coordinate,
                                                        query_location, projected_xy_coordinate,
                                                        nearest_location, ratio);
}

double perpendicularDistanceFromProjectedCoordinate(
    const Coordinate segment_source,
    const Coordinate segment_target,
    const Coordinate query_location,
    const std::pair<double, double> projected_xy_coordinate,
    Coordinate &nearest_location,
    double &ratio)
{
    using namespace coordinate_calculation;

    BOOST_ASSERT(query_location.IsValid());

    // initialize values
    const double x = projected_xy_coordinate.second;
    const double y = projected_xy_coordinate.first;
    const double a = mercator::latToY(toFloating(segment_source.lat));
    const double b = static_cast<double>(toFloating(segment_source.lon));
    const double c = mercator::latToY(toFloating(segment_target.lat));
    const double d = static_cast<double>(toFloating(segment_target.lon));
    double p, q /*,mX*/, new_y;
    if (std::abs(a - c) > std::numeric_limits<double>::epsilon())
    {
        const double m = (d - b) / (c - a); // slope
        // Projection of (x,y) on line joining (a,b) and (c,d)
        p = ((x + (m * y)) + (m * m * a - m * b)) / (1.0 + m * m);
        q = b + m * (p - a);
    }
    else
    {
        p = c;
        q = y;
    }
    new_y = (d * p - c * q) / (a * d - b * c);

    // discretize the result to coordinate precision. it's a hack!
    if (std::abs(new_y) < (1.0 / COORDINATE_PRECISION))
    {
        new_y = 0.0;
    }

    // compute ratio
    ratio = static_cast<double>((p - new_y * a) /
                                c); // These values are actually n/m+n and m/m+n , we need
    // not calculate the explicit values of m an n as we
    // are just interested in the ratio
    if (std::isnan(ratio))
    {
        ratio = (segment_target == query_location ? 1.0 : 0.0);
    }
    else if (std::abs(ratio) <= std::numeric_limits<double>::epsilon())
    {
        ratio = 0.0;
    }
    else if (std::abs(ratio - 1.0) <= std::numeric_limits<double>::epsilon())
    {
        ratio = 1.0;
    }

    // compute nearest location
    BOOST_ASSERT(!std::isnan(ratio));
    if (ratio <= 0.0)
    {
        nearest_location = segment_source;
    }
    else if (ratio >= 1.0)
    {
        nearest_location = segment_target;
    }
    else
    {
        // point lies in between
        nearest_location.lon = toFixed(FloatLongitude(q));
        nearest_location.lat = toFixed(FloatLatitude(mercator::yToLat(p)));
    }
    BOOST_ASSERT(nearest_location.IsValid());

    const double approximate_distance = greatCircleDistance(query_location, nearest_location);
    BOOST_ASSERT(0.0 <= approximate_distance);
    return approximate_distance;
}

double degToRad(const double degree)
{
    using namespace boost::math::constants;
    return degree * (pi<double>() / 180.0);
}

double radToDeg(const double radian)
{
    using namespace boost::math::constants;
    return radian * (180.0 * (1. / pi<double>()));
}

double bearing(const Coordinate first_coordinate, const Coordinate second_coordinate)
{
    const double lon_diff =
        static_cast<double>(toFloating(second_coordinate.lon - first_coordinate.lon));
    const double lon_delta = degToRad(lon_diff);
    const double lat1 = degToRad(static_cast<double>(toFloating(first_coordinate.lat)));
    const double lat2 = degToRad(static_cast<double>(toFloating(second_coordinate.lat)));
    const double y = std::sin(lon_delta) * std::cos(lat2);
    const double x =
        std::cos(lat1) * std::sin(lat2) - std::sin(lat1) * std::cos(lat2) * std::cos(lon_delta);
    double result = radToDeg(std::atan2(y, x));
    while (result < 0.0)
    {
        result += 360.0;
    }

    while (result >= 360.0)
    {
        result -= 360.0;
    }
    return result;
}

double computeAngle(const Coordinate first, const Coordinate second, const Coordinate third)
{
    using namespace boost::math::constants;
    using namespace coordinate_calculation;

    const double v1x = static_cast<double>(toFloating(first.lon - second.lon));
    const double v1y =
        mercator::latToY(toFloating(first.lat)) - mercator::latToY(toFloating(second.lat));
    const double v2x = static_cast<double>(toFloating(third.lon - second.lon));
    const double v2y =
        mercator::latToY(toFloating(third.lat)) - mercator::latToY(toFloating(second.lat));

    double angle = (atan2_lookup(v2y, v2x) - atan2_lookup(v1y, v1x)) * 180. / pi<double>();

    while (angle < 0.)
    {
        angle += 360.;
    }

    return angle;
}

Coordinate interpolateLinear(double factor, const Coordinate from, const Coordinate to)
{
    BOOST_ASSERT(0 <= factor && factor <= 1.0);
    return {
        from.lon +
            toFixed(FloatLongitude(factor * static_cast<double>(toFloating(to.lon - from.lon)))),
        from.lat +
            toFixed(FloatLatitude(factor * static_cast<double>(toFloating(to.lat - from.lat))))};
}

namespace mercator
{
FloatLatitude yToLat(const double value)
{
    using namespace boost::math::constants;

    return FloatLatitude(
        180. * (1. / pi<long double>()) *
        (2. * std::atan(std::exp(value * pi<double>() / 180.)) - half_pi<double>()));
}

double latToY(const FloatLatitude latitude)
{
    using namespace boost::math::constants;

    return 180. * (1. / pi<double>()) *
           std::log(std::tan((pi<double>() / 4.) +
                             static_cast<double>(latitude) * (pi<double>() / 180.) / 2.));
}

} // ns mercato // ns mercatorr
} // ns coordinate_calculation
} // ns util
} // ns osrm

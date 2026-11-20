#include "GeoGrid.h"

#include <math.h>

namespace kdash {
namespace geo {

bool LatLonToGrid(double lat, double lon, int& nx, int& ny) {
  if (lat < 28 || lat > 44 || lon < 120 || lon > 134) return false;
  const double kPi = 3.14159265358979323846;
  const double kDeg = kPi / 180.0;
  const double kRe = 6371.00877, kGrid = 5.0;
  const double kSlat1 = 30.0 * kDeg, kSlat2 = 60.0 * kDeg;
  const double kOlon = 126.0 * kDeg, kOlat = 38.0 * kDeg;
  const double kXo = 43.0, kYo = 136.0;
  double sn = tan(kPi * 0.25 + kSlat2 * 0.5) / tan(kPi * 0.25 + kSlat1 * 0.5);
  sn = log(cos(kSlat1) / cos(kSlat2)) / log(sn);
  double sf = tan(kPi * 0.25 + kSlat1 * 0.5);
  sf = pow(sf, sn) * cos(kSlat1) / sn;
  double ro = tan(kPi * 0.25 + kOlat * 0.5);
  ro = kRe / kGrid * sf / pow(ro, sn);
  double ra = tan(kPi * 0.25 + lat * kDeg * 0.5);
  ra = kRe / kGrid * sf / pow(ra, sn);
  double theta = lon * kDeg - kOlon;
  if (theta > kPi) theta -= 2.0 * kPi;
  if (theta < -kPi) theta += 2.0 * kPi;
  theta *= sn;
  nx = (int)(ra * sin(theta) + kXo + 0.5);
  ny = (int)(ro - ra * cos(theta) + kYo + 0.5);
  return nx >= 1 && nx <= 200 && ny >= 1 && ny <= 200;
}

}  // namespace geo
}  // namespace kdash

#pragma once
// GeoGrid: WGS84 lat/lon -> KMA grid (Lambert conformal conic).
// Constants: RE=6371.00877km, GRID=5.0km, SLAT1=30, SLAT2=60,
// OLON=126, OLAT=38, XO=43, YO=136 (guide original must be re-checked
// before release; 5+ guide appendix points are unit expectations).
// Portable.
namespace kdash {
namespace geo {

// Returns false outside Korea Lambert domain (lat 28..44, lon 120..134).
bool LatLonToGrid(double lat, double lon, int& nx, int& ny);

}  // namespace geo
}  // namespace kdash

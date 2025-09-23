#include <math.h>

#define RADIO_TERRESTRE 6372797.56085
#define GRADOS_RADIANES PI / 180

#define MINTOSEC  60

using namespace std;

double CalGPSDistance(double latitud1, double longitud1, double latitud2, double longitud2);

double CalPerpendicularHeight(double distBtnCorners, double b, double c);

double rad2deg(double rad) ;

//double radians(double deg);

double CalDifangdeg(double x, double y);

double CalBearing(double lat,double lon,double lat2,double lon2);

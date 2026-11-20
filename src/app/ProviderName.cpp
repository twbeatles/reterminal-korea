#include "ProviderName.h"

namespace kdash {
namespace app {

const char* ToString(sched::Provider p) {
  switch (p) {
    case sched::Provider::Sht40: return "SHT40";
    case sched::Provider::KmaNow: return "KMA-NOW";
    case sched::Provider::KmaForecast: return "KMA-FC";
    case sched::Provider::AirKorea: return "AIR";
    case sched::Provider::Exim: return "EXIM";
    case sched::Provider::FscClose: return "FSC";
    case sched::Provider::KisPrice: return "KIS";
    default: return "?";
  }
}

}  // namespace app
}  // namespace kdash

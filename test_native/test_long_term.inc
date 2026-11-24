#include "../src/power/ChargeProtection.h"

namespace {
class ChargerMock : public charge::IRegisters {
 public:
  uint8_t bytes[12] = {};
  int writes=0, fail_read=-1;
  bool fail_write=false, ignore_write=false;
  ChargerMock() { bytes[11]=0x40; bytes[1]=0x1A; bytes[5]=0xBF; bytes[10]=0x80; }
  bool Read(uint8_t r,uint8_t& v) override {
    if (r == fail_read) return false;
    v=bytes[r]; return true;
  }
  bool Write(uint8_t r,uint8_t v) override {
    ++writes;
    if (fail_write) return false;
    if (!ignore_write) bytes[r]=v;
    return true;
  }
};
void ChargeSamples(charge::Protection& p, ChargerMock& m, int mv, int n=3) {
  for (int i=0;i<n;i++) p.Tick(m,true,mv);
}
}

void test_charge_threshold_hysteresis_and_register_preservation() {
  ChargerMock m;
  charge::Protection p;
  ChargeSamples(p,m,4100,2);
  TEST_ASSERT_EQUAL((int)charge::State::Allowed,(int)p.state());
  ChargeSamples(p,m,4100,1);
  TEST_ASSERT_EQUAL((int)charge::State::Limited,(int)p.state());
  TEST_ASSERT_EQUAL_HEX8(0x0A,m.bytes[1]);
  TEST_ASSERT_EQUAL_HEX8(0x8F,m.bytes[5]);  // safety timer/termination untouched
  const int writes=m.writes;
  ChargeSamples(p,m,3820,10);  // between 75 and 80%, retain limit
  TEST_ASSERT_EQUAL(writes,m.writes);
  ChargeSamples(p,m,3700,2);
  TEST_ASSERT_EQUAL((int)charge::State::Limited,(int)p.state());
  ChargeSamples(p,m,3700,1);
  TEST_ASSERT_EQUAL((int)charge::State::Allowed,(int)p.state());
  TEST_ASSERT_EQUAL_HEX8(0x1A,m.bytes[1]);
}

void test_charge_unsupported_id_and_otg_never_write() {
  for (int id : {0,0x41,0x48,0xFF}) {
    ChargerMock m; m.bytes[11]=(uint8_t)id;
    charge::Protection p;
    TEST_ASSERT_EQUAL((int)charge::State::Unsupported,(int)p.Tick(m,true,4100));
    TEST_ASSERT_EQUAL(0,m.writes);
  }
  ChargerMock m; m.bytes[1]|=0x20;
  charge::Protection p;
  TEST_ASSERT_EQUAL((int)charge::State::Fault,(int)p.Tick(m,true,4100));
  TEST_ASSERT_EQUAL(0,m.writes);
}

void test_charge_invalid_samples_reset_confirmation() {
  ChargerMock m; charge::Protection p;
  ChargeSamples(p,m,4100,2);
  TEST_ASSERT_EQUAL((int)charge::State::Fault,(int)p.Tick(m,true,-1));
  ChargeSamples(p,m,4100,1);
  TEST_ASSERT_EQUAL((int)charge::State::Allowed,(int)p.state());
  for (int mv : {0,2499,4501}) {
    const int w=m.writes;
    TEST_ASSERT_EQUAL((int)charge::State::Fault,(int)p.Tick(m,true,mv));
    TEST_ASSERT_EQUAL(w,m.writes);
  }
}

void test_charge_bus_failure_and_write_readback_fail_closed() {
  for (int reg : {1,5,10,11}) {
    ChargerMock m; m.fail_read=reg; charge::Protection p;
    TEST_ASSERT_EQUAL((int)charge::State::Fault,(int)p.Tick(m,true,4100));
    TEST_ASSERT_EQUAL(0,m.writes);
  }
  for (bool ignore : {false,true}) {
    ChargerMock m; m.ignore_write=ignore; m.fail_write=!ignore;
    charge::Protection p;
    TEST_ASSERT_EQUAL((int)charge::State::Fault,(int)p.Tick(m,true,4100));
  }
  ChargerMock m; m.bytes[10]=0; charge::Protection p;
  TEST_ASSERT_EQUAL((int)charge::State::Unknown,(int)p.Tick(m,true,4100));
  TEST_ASSERT_EQUAL(0,m.writes);
}

void test_charge_restart_off_and_config_drift_recovery() {
  ChargerMock m; m.bytes[1]=0x0A; m.bytes[5]=0x8F;
  charge::Protection p;
  TEST_ASSERT_EQUAL((int)charge::State::Limited,(int)p.Tick(m,true,3820));
  m.bytes[1]=0x1A; m.bytes[5]=0xBF;  // simulated PMIC config reset
  TEST_ASSERT_EQUAL((int)charge::State::Limited,(int)p.Tick(m,true,3820));
  TEST_ASSERT_EQUAL_HEX8(0x0A,m.bytes[1]);
  TEST_ASSERT_EQUAL((int)charge::State::Off,(int)p.Tick(m,false,-1));
  TEST_ASSERT_EQUAL_HEX8(0x1A,m.bytes[1]);
  TEST_ASSERT_EQUAL_HEX8(0x8F,m.bytes[5]);
}

void test_charge_watchdog_command_not_replayed() {
  ChargerMock m; m.bytes[1]|=0x40; charge::Protection p;
  ChargeSamples(p,m,4100);
  TEST_ASSERT_EQUAL_HEX8(0x0A,m.bytes[1]);
}

void test_settings_v4_protection_migration_and_fingerprint() {
  Settings s=ConfiguredSettings(), out;
  s.device.charge_protect=true;
  uint8_t data[2048];
  const size_t n=settings_codec::Encode(s,data,sizeof(data));
  TEST_ASSERT_TRUE(n>8);
  TEST_ASSERT_TRUE(settings_codec::Decode(data,n,out));
  TEST_ASSERT_TRUE(out.device.charge_protect);
  const uint32_t fingerprint=settings_codec::DataFingerprint(s);
  s.device.charge_protect=false;
  TEST_ASSERT_EQUAL_UINT32(fingerprint,settings_codec::DataFingerprint(s));
  // v5 appends fast_refresh after charge_protect (ADR 0034): v4 = n-1, v3 = n-2.
  data[0]=3;
  TEST_ASSERT_TRUE(settings_codec::Decode(data,n-2,out));
  TEST_ASSERT_FALSE(out.device.charge_protect);
  TEST_ASSERT_EQUAL_UINT32(fingerprint,settings_codec::DataFingerprint(out));
  data[0]=2;
  TEST_ASSERT_TRUE(settings_codec::Decode(data,n-9,out));
  TEST_ASSERT_FALSE(out.device.charge_protect);
  data[0]=4; data[n-2]=2;
  TEST_ASSERT_FALSE(settings_codec::Decode(data,n-1,out));
}

void test_charge_labels_fit_device_font_and_backup_roundtrip() {
  for (auto state : {charge::State::Off,charge::State::Unknown,charge::State::Unsupported,
                     charge::State::Allowed,charge::State::Limited,charge::State::Fault}) {
    TEST_ASSERT_EQUAL(0,font::MissingGlyphs(ui::DefaultSmall(),charge::Label(state)));
  }
  Settings s=ConfiguredSettings(), back;
  s.device.charge_protect=true;
  char json[1500], bad[32];
  TEST_ASSERT_TRUE(portal::ExportBackup(s,json,sizeof(json))>0);
  TEST_ASSERT_EQUAL((int)portal::ImportResult::Ok,
      (int)portal::ImportBackup(json,back,bad,sizeof(bad)));
  TEST_ASSERT_TRUE(back.device.charge_protect);
  TEST_ASSERT_EQUAL((int)portal::ImportResult::BadValue,
      (int)portal::ImportBackup("{\"format\":\"kdash-backup\",\"version\":1,\"device\":{\"charge_protect\":2}}",back,bad,sizeof(bad)));
}

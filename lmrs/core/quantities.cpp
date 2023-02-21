#include "quantities.h"

namespace lmrs::core {

using namespace std::chrono_literals;

static_assert(volts{1}.count() == 1);
static_assert(volts{1}         == volts{1});
static_assert(volts{1}         == millivolts{1000});
static_assert(millivolts{1000} == volts{1});
static_assert(volts{1000}      == kilovolts{1});
static_assert(kilovolts{1}     == volts{1000});

static_assert(1_V == 1_V);
static_assert(1_kV == 1_kV);
static_assert(1_kV == 1'000_V);
static_assert(1_kV == 1'000'000_mV);

static_assert(meters_per_second{10} ==     meters_per_second{10});
static_assert(meters_per_second{10} == kilometers_per_hour  {36});
static_assert( miles_per_hour  {60} == kilometers_per_hour  {96});
static_assert( miles_per_hour  {60} ==     meters_per_second{26});
static_assert( knots           {10} ==     meters_per_second{5});
static_assert(  feet_per_second{10} ==     meters_per_second{3});

static_assert(quantityCast<kilometers_per_hour>(meters_per_second{0}).count() == 0);
static_assert(quantityCast<kilometers_per_hour>(meters_per_second{1}).count() == 4);
static_assert(quantityCast<kilometers_per_hour>(meters_per_second{2}).count() == 7);
static_assert(quantityCast<kilometers_per_hour>(meters_per_second{3}).count() == 11);
static_assert(quantityCast<kilometers_per_hour>(meters_per_second{4}).count() == 14);
static_assert(quantityCast<kilometers_per_hour>(meters_per_second{5}).count() == 18);

static_assert(quantityCast<meters_per_second>(kilometers_per_hour{ 0}).count() == 0);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{ 1}).count() == 0);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{ 2}).count() == 1);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{ 3}).count() == 1);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{ 4}).count() == 1);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{ 5}).count() == 1);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{ 6}).count() == 2);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{ 7}).count() == 2);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{ 9}).count() == 3);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{10}).count() == 3);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{11}).count() == 3);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{12}).count() == 3);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{13}).count() == 4);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{14}).count() == 4);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{15}).count() == 4);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{16}).count() == 4);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{17}).count() == 5);
static_assert(quantityCast<meters_per_second>(kilometers_per_hour{18}).count() == 5);

static_assert(quantityCast<meters_per_second_f>(kilometers_per_hour{9}).count() == 2.5);
static_assert(quantityCast<kilometers_per_hour_f>(meters_per_second{1}).count() == 3.6);

static_assert( 100_cm ==   1_m);
static_assert(50.0_cm == 0.5_m);

static_assert(unit<std::chrono::       hours> != static_cast<const char16_t *>(nullptr));
static_assert(unit<std::chrono::     minutes> != static_cast<const char16_t *>(nullptr));
static_assert(unit<std::chrono::     seconds> != static_cast<const char16_t *>(nullptr));
static_assert(unit<std::chrono::milliseconds> != static_cast<const char16_t *>(nullptr));
static_assert(unit<std::chrono::microseconds> != static_cast<const char16_t *>(nullptr));
static_assert(unit<std::chrono:: nanoseconds> != static_cast<const char16_t *>(nullptr));
static_assert(unit<std::common_type<std::chrono::seconds, std::chrono::nanoseconds>::type>
                                              != static_cast<const char16_t *>(nullptr));

static_assert(unit<std::chrono::duration<quint8>>             != static_cast<const char16_t *>(nullptr));
static_assert(unit<std::chrono::duration<quint8, std::milli>> != static_cast<const char16_t *>(nullptr));
static_assert(unit<std::chrono::duration<quint8, std::micro>> != static_cast<const char16_t *>(nullptr));

static_assert(unit<kilometers_per_hour> != static_cast<const char16_t *>(nullptr));

static_assert(2_km >= 2000_m);
static_assert(2_km >  1999_m);
static_assert(2_km <  2001_m);
static_assert(2_km <= 2000_m);

// FIXME: make this work
//static_assert(273.15_kelvin == 0_celsius)
//static_assert(-273.15_celius == 0_kelvin)
//static_assert(30_km / 1h == 30_km_h)

static_assert(3_m_s * 5s == 15_m);
static_assert(5s * 3_m_s == 15_m);
static_assert(15_m / 3_m_s == 5s);
static_assert(15_m / 5s == 3_m_s);

static_assert(1_m + 2_m == 3_m);
static_assert(1_km + 2_km == 3_km);

static_assert((1_km + 2_km).count() == 3);
static_assert(decltype(1_km + 2_km)::ratio_type::num == kilometers::ratio_type::num);
static_assert(decltype(1_km + 2_km)::ratio_type::den == kilometers::ratio_type::den);

} // namespace lmrs::core

#include "ArmFeatureProbe.hpp"

#include <sstream>

#if defined(__linux__) && defined(__aarch64__)
#include <sys/auxv.h>
#if __has_include(<asm/hwcap.h>)
#include <asm/hwcap.h>
#endif
#endif

std::string armFeatureSummary() {
  std::ostringstream out;

#if defined(__aarch64__)
  bool neon = true;
  bool sve = false;
  bool sve2 = false;
  bool sme = false;
  bool sme2 = false;

#if defined(__linux__)
  const unsigned long hwcap = getauxval(AT_HWCAP);
  const unsigned long hwcap2 = getauxval(AT_HWCAP2);

#ifdef HWCAP_ASIMD
  neon = (hwcap & HWCAP_ASIMD) != 0;
#endif
#ifdef HWCAP_SVE
  sve = (hwcap & HWCAP_SVE) != 0;
#endif
#ifdef HWCAP2_SVE2
  sve2 = (hwcap2 & HWCAP2_SVE2) != 0;
#endif
#ifdef HWCAP2_SME
  sme = (hwcap2 & HWCAP2_SME) != 0;
#endif
#ifdef HWCAP2_SME2
  sme2 = (hwcap2 & HWCAP2_SME2) != 0;
#endif
#endif

  out << "Arm64"
      << " · Neon " << (neon ? "yes" : "no")
      << " · SVE " << (sve ? "yes" : "no")
      << " · SVE2 " << (sve2 ? "yes" : "no")
      << " · SME " << (sme ? "yes" : "no")
      << " · SME2 " << (sme2 ? "yes" : "no");
#else
  out << "non-Arm64 ABI";
#endif

  return out.str();
}

// Developer-only opt-in benchmark; NOT a test, not a production policy.
// Does NOT access task files, user passwords or rclone configuration.
// Built beside the isolated libsodium fixtures; NEVER put into dist/.
#include <sodium.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
constexpr std::size_t kMaximumRamBytes = 256u * 1024u * 1024u;
constexpr unsigned long long kMaximumOps = 3;
constexpr int kRepetitions = 3;

struct Profile {
  const char *name;
  unsigned long long ops;
  std::size_t mem;
};

bool profileApprovedForResearch(const Profile &p) {
  if (p.ops == 0 || p.ops > kMaximumOps || p.mem == 0 ||
      p.mem > kMaximumRamBytes)
    return false;
  const bool interactive =
      p.ops == 2 && p.mem == 64u * 1024u * 1024u;
  const bool moderate =
      p.ops == 3 && p.mem == 256u * 1024u * 1024u;
  return interactive || moderate;
}

bool measure(const Profile &profile) {
  if (!profileApprovedForResearch(profile)) {
    std::cerr << "Unrecognized or unsafe-for-this-benchmark KDF parameters.\n";
    return false;
  }
  // Entirely synthetic, non-sensitive password bytes.
  constexpr char kSyntheticPassword[] = "synthetic-benchmark-password-ONLY";
  std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{};
  std::array<unsigned char, 32> key{};
  randombytes_buf(salt.data(), salt.size());
  std::array<double, kRepetitions> milliseconds{};

  for (int i = 0; i < kRepetitions; ++i) {
    const auto start = std::chrono::steady_clock::now();
    const int result = crypto_pwhash(
        key.data(), key.size(), kSyntheticPassword,
        sizeof(kSyntheticPassword) - 1, salt.data(), profile.ops,
        profile.mem, crypto_pwhash_ALG_ARGON2ID13);
    const auto end = std::chrono::steady_clock::now();
    sodium_memzero(key.data(), key.size());
    if (result != 0) {
      std::cerr << profile.name
                << ": derivation failed; no automatic weaker fallback.\n";
      return false;
    }
    milliseconds[std::size_t(i)] =
        std::chrono::duration<double, std::milli>(end - start).count();
  }
  std::sort(milliseconds.begin(), milliseconds.end());
  std::cout << profile.name << ",ops=" << profile.ops
            << ",memory_MiB=" << profile.mem / (1024u * 1024u)
            << ",min_ms=" << std::fixed << std::setprecision(1)
            << milliseconds.front() << ",median_ms="
            << milliseconds[std::size_t(kRepetitions / 2)]
            << ",max_ms=" << milliseconds.back() << '\n';
  return true;
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 2 || std::string(argv[1]) != "--run-synthetic") {
    std::cerr
        << "Developer research only; requires --run-synthetic.\n"
        << "Uses at most 256 MiB per derivation, three repetitions per profile.\n"
        << "Never use actual passwords or task files.\n";
    return 2;
  }
  if (sodium_init() < 0) {
    std::cerr << "libsodium initialization failed.\n";
    return 3;
  }
  const Profile profiles[]{
      {"INTERACTIVE", crypto_pwhash_OPSLIMIT_INTERACTIVE,
       crypto_pwhash_MEMLIMIT_INTERACTIVE},
      {"MODERATE", crypto_pwhash_OPSLIMIT_MODERATE,
       crypto_pwhash_MEMLIMIT_MODERATE}};
  for (const Profile &p : profiles)
    if (!profileApprovedForResearch(p)) {
      std::cerr << "Expected libsodium presets changed; refusing to run.\n";
      return 4;
    }
  std::cout << "research-only,libsodium=" << sodium_version_string()
            << ",algorithm=ARGON2ID13,repetitions=" << kRepetitions << '\n';
  for (const Profile &p : profiles)
    if (!measure(p))
      return 5;
  std::cout << "Measured results are NOT a production KDF policy.\n";
  return 0;
}

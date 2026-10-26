// RESEARCH ONLY — pure policy simulations and an optional OS lock probe.
// No actual user password, RcloneBrowser tasks, or release integration.
#include <QtTest>
#include <sodium.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <type_traits>

namespace {

// Distinguish a *failed* attempt from never checking at all. Merely calling
// sodium_malloc() is NOT evidence that the memory is actually mlocked.
enum class LockAttempt { NotAttempted, Failed, Succeeded };
enum class LockPolicy {
  Strict,           // no operation without confirmed locking
  BestEffort,       // allow but explicitly record a degraded protection
  RecoveryOnly      // never create/migrate if lock unavailable
};
enum class Purpose { CreateOrMigrate, RecoverExisting };

struct PolicyDecision {
  bool allowed = false;
  bool mayClaimLocked = false;
  bool degraded = false;
};

// This is a *decision-table proposal*, NOT approved production behavior.
// No method here changes privileges or OS security settings.
PolicyDecision evaluate(LockPolicy policy, Purpose purpose,
                        LockAttempt attempt) noexcept {
  if (attempt == LockAttempt::NotAttempted)
    return {}; // never present a skipped probe as successful protection
  if (attempt == LockAttempt::Succeeded)
    return {true, true, false};

  // The attempted page lock failed. Claiming mlock success would be a lie.
  if (policy == LockPolicy::Strict)
    return {};
  if (policy == LockPolicy::RecoveryOnly &&
      purpose == Purpose::CreateOrMigrate)
    return {};
  return {true, false, true};
}

static_assert(std::is_trivially_copyable_v<PolicyDecision>);
} // namespace

class TestPortableMemoryLockPolicy : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { QVERIFY(sodium_init() >= 0); }

  void neverMistakeUnattemptedLockForProtection() {
    for (const LockPolicy policy : {
             LockPolicy::Strict, LockPolicy::BestEffort,
             LockPolicy::RecoveryOnly}) {
      for (const Purpose purpose : {Purpose::CreateOrMigrate,
                                    Purpose::RecoverExisting}) {
        const auto d = evaluate(policy, purpose, LockAttempt::NotAttempted);
        QVERIFY(!d.allowed);
        QVERIFY(!d.mayClaimLocked);
        QVERIFY(!d.degraded);
      }
    }
  }

  void aSuccessfulExplicitLockAllowsAllCandidateModes() {
    for (const LockPolicy policy : {
             LockPolicy::Strict, LockPolicy::BestEffort,
             LockPolicy::RecoveryOnly}) {
      for (const Purpose purpose : {Purpose::CreateOrMigrate,
                                    Purpose::RecoverExisting}) {
        const auto d = evaluate(policy, purpose, LockAttempt::Succeeded);
        QVERIFY(d.allowed);
        QVERIFY(d.mayClaimLocked);
        QVERIFY(!d.degraded);
      }
    }
  }

  void strictPolicyRejectsLockFailureIncludingRestore() {
    for (const Purpose purpose : {Purpose::CreateOrMigrate,
                                  Purpose::RecoverExisting}) {
      const auto d = evaluate(LockPolicy::Strict, purpose,
                              LockAttempt::Failed);
      QVERIFY(!d.allowed);
      QVERIFY(!d.mayClaimLocked);
      QVERIFY(!d.degraded);
    }
  }

  void bestEffortExplicitlyLabelsDegradedProtection() {
    for (const Purpose purpose : {Purpose::CreateOrMigrate,
                                  Purpose::RecoverExisting}) {
      const auto d = evaluate(LockPolicy::BestEffort, purpose,
                              LockAttempt::Failed);
      QVERIFY(d.allowed);
      QVERIFY(!d.mayClaimLocked);
      QVERIFY(d.degraded);
    }
  }

  void recoveryOnlyNeverWeakensNewBackupCreation() {
    const auto create = evaluate(LockPolicy::RecoveryOnly,
                                 Purpose::CreateOrMigrate,
                                 LockAttempt::Failed);
    QVERIFY(!create.allowed);
    QVERIFY(!create.mayClaimLocked);
    QVERIFY(!create.degraded);

    const auto recover = evaluate(LockPolicy::RecoveryOnly,
                                  Purpose::RecoverExisting,
                                  LockAttempt::Failed);
    QVERIFY(recover.allowed);
    QVERIFY(!recover.mayClaimLocked);
    QVERIFY(recover.degraded);

    // "Recovery" would be a deliberate user-initiated action with a UI
    // warning in a hypothetical product. None is implemented here.
  }

  void optionalRealOsLockProbeUsesOnlyInventedBytes() {
    // Probe sodium_mlock() on a *separate plain heap allocation*: do not
    // double-lock an existing sodium_malloc allocation and then accidentally
    // mismatch its sodium_free() unlock accounting. No real secrets here.
    constexpr std::size_t kSize = 32;
    using HeapBuffer = std::unique_ptr<unsigned char, decltype(&std::free)>;
    HeapBuffer region(static_cast<unsigned char *>(std::malloc(kSize)),
                      &std::free);
    QVERIFY(region != nullptr);
    sodium_memzero(region.get(), kSize);

    const int rc = sodium_mlock(region.get(), kSize);
    if (rc != 0) {
      // CI resources differ from those of end-user Windows/Linux machines;
      // a skipped OS probe does not represent verified locking.
      QSKIP("OS memory locking unavailable; deterministic policy tests still run");
    }
    // On success, confirm the corresponding release API zeros the region.
    region.get()[0] = 0x5a; // synthetic constant
    QCOMPARE(sodium_munlock(region.get(), kSize), 0);
    for (std::size_t i = 0; i < kSize; ++i)
      QCOMPARE(region.get()[i], static_cast<unsigned char>(0));
  }
};
QTEST_GUILESS_MAIN(TestPortableMemoryLockPolicy)
#include "test_portable_memory_lock_policy.moc"

// RESEARCH ONLY — pure policy simulations and an optional OS lock probe.
// No actual user password, RcloneBrowser tasks, or release integration.
#include <QtTest>
#include <sodium.h>
#include "portable_recovery_lock_policy.h"

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <type_traits>

namespace {

using rb_research_lock::LockAttempt;
using rb_research_lock::LockPolicy;
using rb_research_lock::Purpose;
using rb_research_lock::PolicyDecision;
using rb_research_lock::evaluate;

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

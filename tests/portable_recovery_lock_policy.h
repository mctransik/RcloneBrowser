#pragma once
// RESEARCH ONLY. Shared, deterministic decision matrix for disposable CTest
// fixtures; not a security or access-control policy in RcloneBrowser itself.
// A successful sodium_malloc() is NOT proof that mlock/VirtualLock succeeded.
namespace rb_research_lock {
enum class LockAttempt { NotAttempted, Failed, Succeeded };
enum class LockPolicy { Strict, BestEffort, RecoveryOnly };
enum class Purpose { CreateOrMigrate, RecoverExisting };

struct PolicyDecision {
  bool allowed = false;
  bool mayClaimLocked = false;
  bool degraded = false;
};

inline PolicyDecision evaluate(LockPolicy policy, Purpose purpose,
                               LockAttempt attempt) noexcept {
  // Without an explicit OS lock result, no candidate may claim safety.
  if (attempt == LockAttempt::NotAttempted)
    return {};
  if (attempt == LockAttempt::Succeeded)
    return {true, true, false};
  // Failed lock must never be reported as a successfully protected page.
  if (policy == LockPolicy::Strict)
    return {};
  if (policy == LockPolicy::RecoveryOnly &&
      purpose == Purpose::CreateOrMigrate)
    return {};
  return {true, false, true};
}
} // namespace rb_research_lock

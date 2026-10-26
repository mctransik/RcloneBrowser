// RESEARCH ONLY: a small, non-copyable libsodium guarded-key experiment.
// Never linked into RcloneBrowser itself; never handles real passwords/tasks.
// Do not confuse mlocked/guarded memory with protection from a live compromise.
#include <QtTest>

#include <sodium.h>

#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>

namespace {
constexpr std::size_t kKeyBytes = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
static_assert(kKeyBytes == 32, "Guarded-key research assumes 32-byte keys");

using AllocateFn = void *(*)(std::size_t);
using FreeFn = void (*)(void *);
using ProtectFn = int (*)(void *);

// A *test-only* owner: only the pointer is stored in ordinary C++ objects.
// The synthetic key bytes remain in one libsodium allocation, not QByteArray.
// No copy/move operations are allowed; the pointer is freed exactly once.
// Exposed pointers must NEVER escape the lifetime of this object.
class ResearchGuardedKey final {
public:
  enum class Mode { ReadWrite, ReadOnly, NoAccess };

  ResearchGuardedKey(const ResearchGuardedKey &) = delete;
  ResearchGuardedKey &operator=(const ResearchGuardedKey &) = delete;
  ResearchGuardedKey(ResearchGuardedKey &&) = delete;
  ResearchGuardedKey &operator=(ResearchGuardedKey &&) = delete;

  // Test injection is for deterministic allocation failures and exact
  // deallocation counting. In production, injected allocators would need
  // a much stricter contract than this research helper.
  static std::unique_ptr<ResearchGuardedKey> create(
      AllocateFn allocate = sodium_malloc,
      FreeFn deallocate = sodium_free) noexcept {
    if (!allocate || !deallocate || sodium_init() < 0)
      return {};
    void *raw = allocate(kKeyBytes);
    if (!raw)
      return {};
    auto *object = new (std::nothrow)
        ResearchGuardedKey(static_cast<unsigned char *>(raw), deallocate);
    if (!object) {
      // Allocation succeeded but creation of the C++ owner failed.
      // Do not abandon an allocated secret region.
      sodium_memzero(raw, kKeyBytes);
      deallocate(raw);
      return {};
    }
    // sodium_malloc intentionally starts with nonzero garbage bytes.
    sodium_memzero(raw, kKeyBytes);
    return std::unique_ptr<ResearchGuardedKey>(object);
  }

  ~ResearchGuardedKey() noexcept {
    // sodium_free() restores guarded-page access if necessary, checks its
    // canary, clears the region and releases/unlocks it. NEVER read it after.
    deallocate_(data_);
  }

  Mode mode() const noexcept { return mode_; }

  unsigned char *writable() noexcept {
    return mode_ == Mode::ReadWrite ? data_ : nullptr;
  }

  const unsigned char *readable() const noexcept {
    return mode_ == Mode::NoAccess ? nullptr : data_;
  }

  // A real failure leaves the current access mode unchanged and must be
  // handled by the caller. Injection only simulates failure with NO effects.
  bool makeReadOnly(ProtectFn change = sodium_mprotect_readonly) noexcept {
    if (!change)
      return false;
    if (mode_ == Mode::ReadOnly)
      return true;
    if (change(data_) != 0)
      return false;
    mode_ = Mode::ReadOnly;
    return true;
  }

  bool makeNoAccess(ProtectFn change = sodium_mprotect_noaccess) noexcept {
    if (!change)
      return false;
    if (mode_ == Mode::NoAccess)
      return true;
    if (change(data_) != 0)
      return false;
    mode_ = Mode::NoAccess;
    return true;
  }

  bool makeReadWrite(ProtectFn change = sodium_mprotect_readwrite) noexcept {
    if (!change)
      return false;
    if (mode_ == Mode::ReadWrite)
      return true;
    if (change(data_) != 0)
      return false;
    mode_ = Mode::ReadWrite;
    return true;
  }

private:
  ResearchGuardedKey(unsigned char *data, FreeFn deallocate) noexcept
      : data_(data), deallocate_(deallocate) {}
  unsigned char *const data_;
  FreeFn const deallocate_;
  Mode mode_ = Mode::ReadWrite;
};

static_assert(!std::is_copy_constructible_v<ResearchGuardedKey>);
static_assert(!std::is_copy_assignable_v<ResearchGuardedKey>);
static_assert(!std::is_move_constructible_v<ResearchGuardedKey>);
static_assert(!std::is_move_assignable_v<ResearchGuardedKey>);

unsigned int allocCalls = 0;
unsigned int freeCalls = 0;

void *alwaysFailAllocation(std::size_t size) {
  ++allocCalls;
  Q_UNUSED(size);
  return nullptr;
}
void *countingAllocation(std::size_t size) {
  ++allocCalls;
  return sodium_malloc(size);
}
void countingFree(void *data) {
  ++freeCalls;
  sodium_free(data);
}
int denyProtection(void *data) {
  Q_UNUSED(data);
  return -1;
}
} // namespace

class TestPortableGuardedKey final : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { QVERIFY(sodium_init() >= 0); }

  void allocationFailureNeverCreatesAnOwnerOrCallsFree() {
    allocCalls = freeCalls = 0;
    auto key = ResearchGuardedKey::create(alwaysFailAllocation, countingFree);
    QVERIFY(!key);
    QCOMPARE(allocCalls, 1u);
    QCOMPARE(freeCalls, 0u);
    QVERIFY(!ResearchGuardedKey::create(nullptr, countingFree));
    QVERIFY(!ResearchGuardedKey::create(countingAllocation, nullptr));
    QCOMPARE(allocCalls, 1u);
    QCOMPARE(freeCalls, 0u);
  }

  void guardedOwnerInitializesZerosAndFreesOnce() {
    allocCalls = freeCalls = 0;
    {
      auto key = ResearchGuardedKey::create(countingAllocation, countingFree);
      QVERIFY2(key != nullptr, "libsodium could not allocate guarded memory");
      QVERIFY(key->writable() != nullptr);
      QVERIFY(key->readable() != nullptr);
      QCOMPARE(key->mode(), ResearchGuardedKey::Mode::ReadWrite);
      for (std::size_t i = 0; i < kKeyBytes; ++i)
        QCOMPARE(key->readable()[i], static_cast<unsigned char>(0));
      sodium_memzero(key->writable(), kKeyBytes);
      sodium_memzero(key->writable(), kKeyBytes); // double wipe is safe
      key->writable()[0] = 0x42; // PUBLIC synthetic test byte only
      QCOMPARE(key->readable()[0], static_cast<unsigned char>(0x42));
      QCOMPARE(freeCalls, 0u);
    }
    QCOMPARE(allocCalls, 1u);
    QCOMPARE(freeCalls, 1u);
    // No read-after-free: sodium_free guarantees the cleanup contract, but
    // observing actual released pages would be invalid/undefined behavior.
  }

  void injectedProtectionFailuresDoNotLieAboutAccessMode() {
    auto key = ResearchGuardedKey::create();
    QVERIFY(key != nullptr);
    QVERIFY(!key->makeReadOnly(denyProtection));
    QCOMPARE(key->mode(), ResearchGuardedKey::Mode::ReadWrite);
    QVERIFY(key->writable() != nullptr);
    QVERIFY(!key->makeNoAccess(denyProtection));
    QCOMPARE(key->mode(), ResearchGuardedKey::Mode::ReadWrite);
    QVERIFY(!key->makeReadOnly(nullptr));
    QVERIFY(!key->makeNoAccess(nullptr));
    QVERIFY(!key->makeReadWrite(nullptr));
    QCOMPARE(key->mode(), ResearchGuardedKey::Mode::ReadWrite);
  }

  void protectionTransitionsWhenSupported() {
    auto key = ResearchGuardedKey::create();
    QVERIFY(key != nullptr);
    key->writable()[0] = 0x35; // synthetic constant
    // Sandboxes and platforms may not permit mprotect. Do not deliberately
    // dereference protected pages or crash CI. Production would fail closed
    // if protection is a required policy property.
    if (!key->makeReadOnly())
      QSKIP("libsodium read-only page protection unavailable here");
    QCOMPARE(key->mode(), ResearchGuardedKey::Mode::ReadOnly);
    QVERIFY(key->writable() == nullptr);
    QVERIFY(key->readable() != nullptr);
    QCOMPARE(key->readable()[0], static_cast<unsigned char>(0x35));
    QVERIFY(!key->makeNoAccess(denyProtection));
    QCOMPARE(key->mode(), ResearchGuardedKey::Mode::ReadOnly);

    if (!key->makeNoAccess())
      QSKIP("libsodium no-access page protection unavailable here");
    QCOMPARE(key->mode(), ResearchGuardedKey::Mode::NoAccess);
    QVERIFY(key->readable() == nullptr);
    QVERIFY(key->writable() == nullptr);
    QVERIFY(!key->makeReadWrite(denyProtection));
    QCOMPARE(key->mode(), ResearchGuardedKey::Mode::NoAccess);
    if (!key->makeReadWrite())
      QSKIP("libsodium read-write restoration unavailable here");
    QCOMPARE(key->mode(), ResearchGuardedKey::Mode::ReadWrite);
    QVERIFY(key->writable() != nullptr);
    QCOMPARE(key->readable()[0], static_cast<unsigned char>(0x35));
  }

  void protectedAllocationCanBeSafelyFreed() {
    // Free may restore noaccess and erase internally. We intentionally do
    // NOT dereference inaccessible memory or assert a post-free byte value.
    allocCalls = freeCalls = 0;
    {
      auto key = ResearchGuardedKey::create(countingAllocation, countingFree);
      QVERIFY(key != nullptr);
      key->writable()[0] = 0x42;
      if (!key->makeNoAccess())
        QSKIP("Page protection unavailable; noaccess release not tested");
      QVERIFY(key->readable() == nullptr);
    }
    QCOMPARE(allocCalls, 1u);
    QCOMPARE(freeCalls, 1u);
  }
};
QTEST_GUILESS_MAIN(TestPortableGuardedKey)
#include "test_portable_guarded_key.moc"

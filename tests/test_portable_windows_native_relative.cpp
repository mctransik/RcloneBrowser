// Windows-only synthetic native RootDirectory research. No application linkage.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>

namespace {
class Handle {
 public:
  explicit Handle(HANDLE h = INVALID_HANDLE_VALUE): h_(h) {}
  ~Handle() { if (valid()) CloseHandle(h_); }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  bool valid() const { return h_ && h_ != INVALID_HANDLE_VALUE; }
  HANDLE get() const { return h_; }
 private:
  HANDLE h_;
};
using NtCreateFileFn = NTSTATUS (NTAPI *)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES,
    PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
bool writeFixture(const QString& path, const QByteArray& bytes) {
  QFile f(path);
  return f.open(QIODevice::WriteOnly | QIODevice::NewOnly)
      && f.write(bytes) == bytes.size() && f.flush();
}
QByteArray readHandle(HANDLE h) {
  char buf[32] = {};
  DWORD got = 0;
  if (!ReadFile(h, buf, sizeof(buf), &got, nullptr))
    return {};
  return QByteArray(buf, int(got));
}
}
class WindowsNativeRelativeResearch : public QObject {
  Q_OBJECT
 private slots:
  void verifiedPayloadPublishesOnUnchangedSyntheticTarget() {
    // This test validates a known, public fixture digest BEFORE staging.
    // QSaveFile then publishes that data on a non-hostile, fixed test path.
    // It does NOT prove that QSaveFile's pathname-based commit is safe from
    // concurrent substitutions or that staged disk bytes are reverified.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir root(temp.path());
    QVERIFY(root.mkdir("approved"));
    QVERIFY(root.mkdir("outside"));
    const QString target = root.filePath("approved/restored.txt");
    const QString outside = root.filePath("outside/sentinel.txt");
    const QByteArray oldBytes("PREVIOUS PUBLIC DATA");
    const QByteArray outsideBytes("OUTSIDE PUBLIC SENTINEL");
    const QByteArray verified("VERIFIED RESTORE CONTENT - RESEARCH ONLY");
    const QByteArray expectedSha256 = QByteArray::fromHex(
        "c158f24b5d37bf6e815b7eefbcc23f92077ae370a658ae23193d7dc9d864cb57");
    QVERIFY(writeFixture(target, oldBytes));
    QVERIFY(writeFixture(outside, outsideBytes));
    QCOMPARE(QCryptographicHash::hash(verified, QCryptographicHash::Sha256),
             expectedSha256);

    {
      QSaveFile staging(target);
      staging.setDirectWriteFallback(false);
      QVERIFY2(staging.open(QIODevice::WriteOnly), "Staging file not created");
      QCOMPARE(staging.write(verified), qint64(verified.size()));
      QVERIFY(staging.flush());
      QVERIFY2(staging.commit(), "Uncontended fixture commit failed");
    }

    QFile published(target), preservedOutside(outside);
    QVERIFY(published.open(QIODevice::ReadOnly));
    const QByteArray publishedBytes = published.readAll();
    QCOMPARE(publishedBytes, verified);
    QCOMPARE(QCryptographicHash::hash(publishedBytes, QCryptographicHash::Sha256),
             expectedSha256);
    QVERIFY(preservedOutside.open(QIODevice::ReadOnly));
    QCOMPARE(preservedOutside.readAll(), outsideBytes);
    // Only an uncontended Qt staging/commit smoke test; no production restore.
  }

  void swappedDestinationIdentityCancelsVerifiedStaging() {
    // A previously checked target file can be swapped before QSaveFile
    // commits: a stale path lookup MUST NOT authorize replacement. This
    // research test detects the substitution and cancels staging; it does
    // NOT implement an atomic compare-and-swap commit.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir root(temp.path());
    QVERIFY(root.mkdir("approved"));
    QVERIFY(root.mkdir("outside"));
    const QString target = root.filePath("approved/restored.txt");
    const QString previous = root.filePath("approved/previous.txt");
    const QString outside = root.filePath("outside/sentinel.txt");
    const QByteArray oldBytes("ORIGINAL VERIFIED PUBLIC FILE");
    const QByteArray swappedBytes("SIMULATED EXTERNAL REPLACEMENT");
    const QByteArray outsideBytes("OUTSIDE UNCHANGED");
    const QByteArray stagedBytes("VERIFIED RESTORE CONTENT - RESEARCH ONLY");
    QVERIFY(writeFixture(target, oldBytes));
    QVERIFY(writeFixture(outside, outsideBytes));

    auto inspectNoFollow = [](const QString &path,
                              BY_HANDLE_FILE_INFORMATION *identity,
                              FILE_ATTRIBUTE_TAG_INFO *tag) -> bool {
      Handle opened(CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),
          FILE_READ_ATTRIBUTES,
          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
          nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
      return opened.valid() &&
          GetFileInformationByHandle(opened.get(), identity) &&
          GetFileInformationByHandleEx(opened.get(), FileAttributeTagInfo,
                                     tag, sizeof(*tag));
    };
    BY_HANDLE_FILE_INFORMATION originalIdentity{}, replacedIdentity{};
    FILE_ATTRIBUTE_TAG_INFO originalTag{}, replacedTag{};
    QVERIFY(inspectNoFollow(target, &originalIdentity, &originalTag));
    QVERIFY(!(originalTag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT));

    QSaveFile staging(target);
    staging.setDirectWriteFallback(false);
    QVERIFY(staging.open(QIODevice::WriteOnly));
    QCOMPARE(staging.write(stagedBytes), qint64(stagedBytes.size()));
    QVERIFY(staging.flush());

    // Deterministic substitution immediately before the planned commit.
    // Leave no target inspection handle open while renaming on Windows.
    QVERIFY2(root.rename("approved/restored.txt", "approved/previous.txt"),
             "Synthetic target could not be renamed after staging");
    QVERIFY(writeFixture(target, swappedBytes));
    QVERIFY(inspectNoFollow(target, &replacedIdentity, &replacedTag));
    const bool sameIdentity =
        originalIdentity.dwVolumeSerialNumber == replacedIdentity.dwVolumeSerialNumber &&
        originalIdentity.nFileIndexHigh == replacedIdentity.nFileIndexHigh &&
        originalIdentity.nFileIndexLow == replacedIdentity.nFileIndexLow;
    QVERIFY2(!sameIdentity, "Path replacement was not detected by file identity");
    QVERIFY(!(replacedTag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT));

    // A candidate restore policy aborts whenever its target's identity
    // changes. Never call commit() successfully after such a mismatch.
    staging.cancelWriting();
    QVERIFY2(!staging.commit(), "Cancelled staging unexpectedly published data");

    QFile current(target), original(previous), untouched(outside);
    QVERIFY(current.open(QIODevice::ReadOnly));
    QCOMPARE(current.readAll(), swappedBytes);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), oldBytes);
    QVERIFY(untouched.open(QIODevice::ReadOnly));
    QCOMPARE(untouched.readAll(), outsideBytes);
    const QStringList names = QDir(root.filePath("approved")).entryList(
        QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
    QCOMPARE(names, (QStringList{"previous.txt", "restored.txt"}));
    // TOCTOU still exists BETWEEN this identity check and any path-based
    // commit. Do not integrate QSaveFile as a handle-bound secure restore.
  }

  void missingRelativeLeafFailsClosedAndRecoveryWorks() {
    // Isolated error-path fixture. FILE_OPEN must never create a missing
    // child or silently redirect an operation to a sibling/other directory.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir root(temp.path());
    QVERIFY(root.mkdir("approved"));
    QVERIFY(root.mkdir("outside"));
    const QString approved = root.filePath("approved");
    const QString outside = root.filePath("outside");
    const QString safeFile = QDir(approved).filePath("safe.txt");
    const QString outsideFile = QDir(outside).filePath("sentinel.txt");
    const QByteArray safeBytes("SAFE PUBLIC BYTES");
    const QByteArray outsideBytes("OUTSIDE UNCHANGED");
    QVERIFY(writeFixture(safeFile, safeBytes));
    QVERIFY(writeFixture(outsideFile, outsideBytes));

    Handle parent(CreateFileW(reinterpret_cast<LPCWSTR>(approved.utf16()),
        FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    QVERIFY2(parent.valid(), "Could not pin disposable approved directory");

    HMODULE dll = GetModuleHandleW(L"ntdll.dll");
    QVERIFY(dll != nullptr);
    auto nativeOpen = reinterpret_cast<NtCreateFileFn>(
        GetProcAddress(dll, "NtCreateFile"));
    QVERIFY(nativeOpen != nullptr);

    wchar_t missingName[] = L"missing.txt";
    UNICODE_STRING name{};
    name.Buffer = missingName;
    name.Length = USHORT((sizeof(missingName) / sizeof(wchar_t) - 1)
                          * sizeof(wchar_t));
    name.MaximumLength = USHORT(sizeof(missingName));
    OBJECT_ATTRIBUTES attributes{};
    attributes.Length = sizeof(attributes);
    attributes.RootDirectory = parent.get();
    attributes.ObjectName = &name;
    attributes.Attributes = OBJ_CASE_INSENSITIVE;
    constexpr ULONG kFileOpen = 1;
    constexpr ULONG kSynchronous = 0x20;
    constexpr ULONG kNonDirectory = 0x40;
    constexpr ULONG kNoFollow = 0x00200000;

    IO_STATUS_BLOCK missingIos{};
    HANDLE missingRaw = INVALID_HANDLE_VALUE;
    const NTSTATUS missingStatus = nativeOpen(&missingRaw,
        FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        &attributes, &missingIos, nullptr, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        kFileOpen, kSynchronous | kNonDirectory | kNoFollow, nullptr, 0);
    {
      Handle failed(missingRaw);
      QVERIFY2(missingStatus < 0,
               "Missing leaf was unexpectedly opened rather than rejected");
      QVERIFY2(!failed.valid(), "Failed lookup returned a valid file handle");
    }
    QVERIFY(!QFileInfo::exists(QDir(approved).filePath("missing.txt")));

    // An unrelated subsequent valid open must still work against the SAME
    // pinned parent handle; failure must not poison or redirect it.
    wchar_t safeName[] = L"safe.txt";
    name.Buffer = safeName;
    name.Length = USHORT((sizeof(safeName) / sizeof(wchar_t) - 1)
                          * sizeof(wchar_t));
    name.MaximumLength = USHORT(sizeof(safeName));
    IO_STATUS_BLOCK safeIos{};
    HANDLE safeRaw = INVALID_HANDLE_VALUE;
    const NTSTATUS safeStatus = nativeOpen(&safeRaw,
        FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        &attributes, &safeIos, nullptr, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        kFileOpen, kSynchronous | kNonDirectory | kNoFollow, nullptr, 0);
    Handle safe(safeRaw);
    QVERIFY2(safeStatus >= 0 && safe.valid(),
             "Valid relative file could not be opened after earlier failure");
    QCOMPARE(readHandle(safe.get()), safeBytes);
    QFile external(outsideFile);
    QVERIFY(external.open(QIODevice::ReadOnly));
    QCOMPARE(external.readAll(), outsideBytes);
    // No production writes or fail-safe restore behavior are claimed here.
  }

  void cancelledStagingLeavesNoPublishedPartialFile() {
    // Research-only synthetic crash/abort injection with QSaveFile: a
    // partial staging write must never be published. This is NOT a proof
    // of atomic restore durability, secure path resolution or ACL safety.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir root(temp.path());
    QVERIFY(root.mkdir("approved"));
    QVERIFY(root.mkdir("outside"));
    const QString approved = root.filePath("approved");
    const QString existingFile = QDir(approved).filePath("restored.txt");
    const QString newFile = QDir(approved).filePath("new-restore.txt");
    const QString sentinelFile = root.filePath("outside/sentinel.txt");
    const QByteArray previousBytes("PREVIOUS VERIFIED PUBLIC DATA");
    const QByteArray sentinelBytes("OUTSIDE MUST REMAIN UNCHANGED");
    QVERIFY(writeFixture(existingFile, previousBytes));
    QVERIFY(writeFixture(sentinelFile, sentinelBytes));

    for (const QString &destination : {existingFile, newFile}) {
      const QStringList before = QDir(approved).entryList(
          QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
      const bool existedBefore = QFileInfo::exists(destination);
      {
        QSaveFile staging(destination);
        QVERIFY2(staging.open(QIODevice::WriteOnly),
                 "Could not create temporary staged test file");
        const QByteArray partial("TRUNCATED INCOMPLETE CONTENT");
        QCOMPARE(staging.write(partial), qint64(partial.size()));
        // Simulate interruption before verification or publication.
        staging.cancelWriting();
        QVERIFY2(!staging.commit(),
                 "A deliberately cancelled staging write was published");
      }
      QCOMPARE(QFileInfo::exists(destination), existedBefore);
      QCOMPARE(QDir(approved).entryList(
          QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name), before);
      QFile existing(existingFile);
      QVERIFY(existing.open(QIODevice::ReadOnly));
      QCOMPARE(existing.readAll(), previousBytes);
      QFile outside(sentinelFile);
      QVERIFY(outside.open(QIODevice::ReadOnly));
      QCOMPARE(outside.readAll(), sentinelBytes);
    }
    // QSaveFile behavior is verified independently of RcloneBrowser runtime.
  }

  void nestedRelativeOpenDoesNotFollowReplacedAncestor() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir root(temp.path());
    QVERIFY(root.mkpath("approved/nested"));
    const QString approved = root.filePath("approved");
    const QString nested = QDir(approved).filePath("nested");
    QVERIFY(writeFixture(QDir(nested).filePath("marker.txt"), "ORIGINAL-NESTED"));

    Handle pinned(CreateFileW(reinterpret_cast<LPCWSTR>(approved.utf16()),
        FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    QVERIFY(pinned.valid());

    // Replace the ancestor while ONLY the approved root is pinned. Opening
    // the nested child first can prevent a Windows directory rename; the
    // security property under test is traversal AFTER that path replacement.
    QVERIFY2(root.rename("approved", "approved-moved"),
             "Could not rename synthetic ancestor with root handle pinned");
    QVERIFY(root.mkpath("approved/nested"));
    QVERIFY(writeFixture(QDir(root.filePath("approved/nested")).filePath("marker.txt"),
                         "REPLACEMENT-NESTED"));

    HMODULE dll = GetModuleHandleW(L"ntdll.dll");
    QVERIFY(dll != nullptr);
    auto nativeOpen = reinterpret_cast<NtCreateFileFn>(
        GetProcAddress(dll, "NtCreateFile"));
    QVERIFY(nativeOpen != nullptr);

    // Single component only: never pass a multi-component untrusted path.
    wchar_t childName[] = L"nested";
    UNICODE_STRING child{};
    child.Buffer = childName;
    child.Length = USHORT((sizeof(childName) / sizeof(wchar_t) - 1) * sizeof(wchar_t));
    child.MaximumLength = USHORT(sizeof(childName));
    OBJECT_ATTRIBUTES attrs{};
    attrs.Length = sizeof(attrs);
    attrs.RootDirectory = pinned.get();
    attrs.ObjectName = &child;
    attrs.Attributes = OBJ_CASE_INSENSITIVE;
    IO_STATUS_BLOCK ios{};
    HANDLE childRaw = INVALID_HANDLE_VALUE;
    constexpr ULONG openExisting = 1;
    constexpr ULONG synchronous = 0x20;
    constexpr ULONG noFollowFinal = 0x00200000;
    constexpr ULONG directoryOnly = 0x1;
    NTSTATUS result = nativeOpen(&childRaw,
        FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        &attrs, &ios, nullptr, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        openExisting, synchronous | noFollowFinal | directoryOnly, nullptr, 0);
    Handle pinnedChild(childRaw);
    QVERIFY2(result >= 0 && pinnedChild.valid(),
             "Failed to open nested directory relative to approved root");

    FILE_ATTRIBUTE_TAG_INFO tag{};
    QVERIFY(GetFileInformationByHandleEx(pinnedChild.get(), FileAttributeTagInfo,
                                        &tag, sizeof(tag)));
    QVERIFY2((tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0,
             "Nested component unexpectedly is a reparse point");

    wchar_t filename[] = L"marker.txt";
    UNICODE_STRING leaf{};
    leaf.Buffer = filename;
    leaf.Length = USHORT((sizeof(filename) / sizeof(wchar_t) - 1) * sizeof(wchar_t));
    leaf.MaximumLength = USHORT(sizeof(filename));
    OBJECT_ATTRIBUTES leafAttrs{};
    leafAttrs.Length = sizeof(leafAttrs);
    leafAttrs.RootDirectory = pinnedChild.get();
    leafAttrs.ObjectName = &leaf;
    leafAttrs.Attributes = OBJ_CASE_INSENSITIVE;
    IO_STATUS_BLOCK leafIos{};
    HANDLE leafRaw = INVALID_HANDLE_VALUE;
    result = nativeOpen(&leafRaw, FILE_READ_DATA | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
        &leafAttrs, &leafIos, nullptr, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        openExisting, synchronous | noFollowFinal, nullptr, 0);
    Handle leafHandle(leafRaw);
    QVERIFY2(result >= 0 && leafHandle.valid(),
             "Failed to open leaf relative to pinned nested handle");
    QCOMPARE(readHandle(leafHandle.get()), QByteArray("ORIGINAL-NESTED"));
    QFile replacement(root.filePath("approved/nested/marker.txt"));
    QVERIFY(replacement.open(QIODevice::ReadOnly));
    QCOMPARE(replacement.readAll(), QByteArray("REPLACEMENT-NESTED"));
    // Research proof only: no writes, no atomic restore, no comprehensive
    // reparse, hardlink, ACL, UNC or hostile-concurrency protection.
  }

  void relativeOpenRemainsBoundToOriginalDirectory() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QDir root(temp.path());
    QVERIFY(root.mkdir("approved"));
    const QString approved = root.filePath("approved");
    QVERIFY(writeFixture(QDir(approved).filePath("marker.txt"), "ORIGINAL"));
    Handle pinned(CreateFileW(reinterpret_cast<LPCWSTR>(approved.utf16()),
       FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE,
       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
       nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS |
       FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    QVERIFY2(pinned.valid(), "Cannot pin disposable directory handle");
    QVERIFY2(root.rename("approved", "approved-moved"),
             "Cannot rename disposable directory with open handle");
    QVERIFY(root.mkdir("approved"));
    QVERIFY(writeFixture(QDir(approved).filePath("marker.txt"), "REPLACEMENT"));

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    QVERIFY2(ntdll != nullptr, "ntdll unavailable");
    auto ntCreateFile = reinterpret_cast<NtCreateFileFn>(
        GetProcAddress(ntdll, "NtCreateFile"));
    QVERIFY2(ntCreateFile != nullptr, "NtCreateFile unavailable");

    wchar_t component[] = L"marker.txt"; // Exactly one trusted synthetic component.
    UNICODE_STRING name{};
    name.Buffer = component;
    name.Length = USHORT((sizeof(component) / sizeof(wchar_t) - 1) * sizeof(wchar_t));
    name.MaximumLength = USHORT(sizeof(component));
    OBJECT_ATTRIBUTES attributes{};
    attributes.Length = sizeof(attributes);
    attributes.RootDirectory = pinned.get();
    attributes.ObjectName = &name;
    attributes.Attributes = OBJ_CASE_INSENSITIVE;
    IO_STATUS_BLOCK ios{};
    HANDLE raw = INVALID_HANDLE_VALUE;
    constexpr ULONG fileOpen = 1;
    constexpr ULONG synchronousNonalert = 0x20;
    constexpr ULONG openReparsePoint = 0x00200000;
    NTSTATUS st = ntCreateFile(&raw, FILE_READ_DATA | FILE_READ_ATTRIBUTES |
       SYNCHRONIZE, &attributes, &ios, nullptr, 0,
       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, fileOpen,
       synchronousNonalert | openReparsePoint, nullptr, 0);
    Handle opened(raw);
    QVERIFY2(st >= 0 && opened.valid(), "Native relative open failed");
    QCOMPARE(readHandle(opened.get()), QByteArray("ORIGINAL"));
    QFile replacement(QDir(approved).filePath("marker.txt"));
    QVERIFY(replacement.open(QIODevice::ReadOnly));
    QCOMPARE(replacement.readAll(), QByteArray("REPLACEMENT"));
    // Proves single-component open from pinned root, NOT recursive secure
    // traversal, reparse rejection of every ancestor or safe restore/commit.
  }
};
QTEST_MAIN(WindowsNativeRelativeResearch)
#include "test_portable_windows_native_relative.moc"

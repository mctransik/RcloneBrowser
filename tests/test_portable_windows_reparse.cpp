// RESEARCH-ONLY, Windows-specific NTFS/reparse-point regression tests.
// Compiled as an isolated QtTest; NEVER linked to RcloneBrowser itself.
// Disposable QTemporaryDir public bytes only. No real passwords or tasks.bin.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>
#include <winternl.h>

#include <QtTest>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstddef>
#include <cstring>
#include <vector>

namespace {

class UniqueHandle final {
 public:
  explicit UniqueHandle(HANDLE h = INVALID_HANDLE_VALUE) : value_(h) {}
  UniqueHandle(const UniqueHandle &) = delete;
  UniqueHandle &operator=(const UniqueHandle &) = delete;
  ~UniqueHandle() { if (valid()) CloseHandle(value_); }
  bool valid() const { return value_ != INVALID_HANDLE_VALUE && value_; }
  HANDLE get() const { return value_; }

 private:
  HANDLE value_;
};

HANDLE openNoFollow(const QString &name, DWORD access) {
  // FILE_FLAG_OPEN_REPARSE_POINT inspects the link itself instead of
  // deliberately traversing it at the LAST path component. Ancestors are
  // NOT secured by this call: this is NOT a TOCTOU-safe path resolver.
  return CreateFileW(reinterpret_cast<LPCWSTR>(name.utf16()), access,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     nullptr, OPEN_EXISTING,
                     FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
                     nullptr);
}

struct ObservedObject {
  DWORD attributes = 0;
  DWORD tag = 0;
  DWORD volume = 0;
  DWORD fileIndexHigh = 0;
  DWORD fileIndexLow = 0;
};

bool inspectOpened(HANDLE h, ObservedObject *out) {
  if (!out || h == INVALID_HANDLE_VALUE || !h)
    return false;
  FILE_ATTRIBUTE_TAG_INFO tagInfo{};
  BY_HANDLE_FILE_INFORMATION identity{};
  if (!GetFileInformationByHandleEx(h, FileAttributeTagInfo,
                                    &tagInfo, sizeof(tagInfo)) ||
      !GetFileInformationByHandle(h, &identity))
    return false;
  *out = {tagInfo.FileAttributes, tagInfo.ReparseTag,
          identity.dwVolumeSerialNumber,
          identity.nFileIndexHigh, identity.nFileIndexLow};
  return true;
}

bool inspectPathNoFollow(const QString &path, ObservedObject *out) {
  UniqueHandle h(openNoFollow(path, FILE_READ_ATTRIBUTES));
  return h.valid() && inspectOpened(h.get(), out);
}

bool sameOpenedIdentity(const ObservedObject &a,
                        const ObservedObject &b) {
  return a.volume == b.volume && a.fileIndexHigh == b.fileIndexHigh &&
         a.fileIndexLow == b.fileIndexLow;
}

// Deliberately conservative candidate: ANY reparse attribute is rejected,
// not just known tags such as mount point or symlink. Actual authorization
// would require secured parent handles and race-resistant namespace walk.
bool candidateRejectReparse(const ObservedObject &value) {
  return (value.attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool inspectParentComponentsNoFollow(const QString &path) {
  QString parent = QFileInfo(path).absolutePath();
  for (int i = 0; i < 128; ++i) {
    ObservedObject observed{};
    if (!inspectPathNoFollow(parent, &observed) ||
        candidateRejectReparse(observed) ||
        (observed.attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
      return false;
    const QString ancestor = QFileInfo(parent).absolutePath();
    if (ancestor == parent)
      return true;
    parent = ancestor;
  }
  return false;
}

bool writePublicBytes(const QString &path, const QByteArray &data) {
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
         file.write(data) == data.size() &&
         file.flush();
}

// User-mode replica of the mount point data layout needed for a disposable
// test junction. REPARSE_DATA_BUFFER is not in every user-mode Windows SDK.
// See MS-FSCC and the REPARSE_DATA_BUFFER documentation: bytes, NOT WCHARs.
struct TestMountPointData {
  DWORD tag;
  WORD dataLength;
  WORD reserved;
  WORD substituteOffset;
  WORD substituteLength;
  WORD printOffset;
  WORD printLength;
  WCHAR path[1];
};
static_assert(offsetof(TestMountPointData, path) == 16);

bool createTemporaryNtfsJunction(const QString &link,
                                 const QString &destination,
                                 DWORD *error) {
  const QString target = QStringLiteral("\\??\\") +
                         QDir::toNativeSeparators(destination);
  const size_t nameBytes = size_t(target.size()) * sizeof(WCHAR);
  if (nameBytes > 65520)
    return false;
  // Include WCHAR terminators for SubstituteName and empty PrintName.
  const size_t bufferSize = offsetof(TestMountPointData, path) +
                            nameBytes + 2 * sizeof(WCHAR);
  std::vector<DWORD> aligned((bufferSize + sizeof(DWORD) - 1) /
                             sizeof(DWORD), 0);
  auto *data = reinterpret_cast<TestMountPointData *>(aligned.data());
  data->tag = IO_REPARSE_TAG_MOUNT_POINT;
  data->dataLength = WORD(bufferSize - 8); // prefix tag/len/reserved = 8
  data->substituteOffset = 0;
  data->substituteLength = WORD(nameBytes);
  data->printOffset = WORD(nameBytes + sizeof(WCHAR));
  data->printLength = 0;
  std::memcpy(data->path, target.utf16(), nameBytes);

  UniqueHandle h(openNoFollow(link, GENERIC_WRITE));
  if (!h.valid()) {
    if (error) *error = GetLastError();
    return false;
  }
  DWORD bytesReturned = 0;
  const BOOL ok = DeviceIoControl(
      h.get(), FSCTL_SET_REPARSE_POINT, data, DWORD(bufferSize),
      nullptr, 0, &bytesReturned, nullptr);
  if (error) *error = ok ? ERROR_SUCCESS : GetLastError();
  return ok != FALSE;
}

bool removeTemporaryJunction(const QString &path) {
  UniqueHandle h(openNoFollow(path, GENERIC_WRITE));
  if (!h.valid())
    return false;
  // Deletion requires only the fixed 8-byte reparse header and tag.
  const DWORD header[2] = {IO_REPARSE_TAG_MOUNT_POINT, 0};
  DWORD returned = 0;
  return DeviceIoControl(h.get(), FSCTL_DELETE_REPARSE_POINT,
                         const_cast<DWORD *>(header), 8,
                         nullptr, 0, &returned, nullptr) != FALSE;
}

} // namespace

class TestPortableWindowsReparse final : public QObject {
  Q_OBJECT
 private slots:
  void ordinaryFileAndParentsUseNativeHandleInspection() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString input = dir.filePath(QStringLiteral("synthetic-only.dat"));
    QVERIFY(writePublicBytes(input, "PUBLIC TEST FILE"));
    ObservedObject file{}, parent{};
    QVERIFY(inspectPathNoFollow(input, &file));
    QVERIFY(!candidateRejectReparse(file));
    QVERIFY(!(file.attributes & FILE_ATTRIBUTE_DIRECTORY));
    QVERIFY(inspectParentComponentsNoFollow(input));
    QVERIFY(inspectPathNoFollow(dir.path(), &parent));
    QVERIFY(!candidateRejectReparse(parent));
    QVERIFY(parent.attributes & FILE_ATTRIBUTE_DIRECTORY);
  }

  void allReparseTagsAreRejectedByCandidate() {
    for (const DWORD tag : {DWORD(IO_REPARSE_TAG_MOUNT_POINT),
                            DWORD(IO_REPARSE_TAG_SYMLINK),
                            DWORD(0x80000000UL), DWORD(0)}) {
      ObservedObject observed{};
      observed.attributes = FILE_ATTRIBUTE_DIRECTORY |
                            FILE_ATTRIBUTE_REPARSE_POINT;
      observed.tag = tag;
      QVERIFY(candidateRejectReparse(observed));
    }
    ObservedObject normal{};
    normal.attributes = FILE_ATTRIBUTE_ARCHIVE;
    QVERIFY(!candidateRejectReparse(normal));
  }

  void actualDisposableNtfsJunctionIsDetectedBeforeUse() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString target = root.filePath(QStringLiteral("real-target"));
    const QString redirect = root.filePath(QStringLiteral("ntfs-junction"));
    QVERIFY(QDir().mkpath(target));
    QVERIFY(QDir().mkpath(redirect));
    const QByteArray sentinel("PUBLIC TARGET MUST REMAIN UNCHANGED");
    const QString targetFile = target + QStringLiteral("/sentinel.txt");
    QVERIFY(writePublicBytes(targetFile, sentinel));

    DWORD winError = ERROR_SUCCESS;
    const bool created = createTemporaryNtfsJunction(
        redirect, target, &winError);
    QVERIFY2(created, qPrintable(QStringLiteral(
        "Could not create mandatory NTFS junction fixture, Win32 error %1")
        .arg(winError)));

    ObservedObject linked{};
    QVERIFY(inspectPathNoFollow(redirect, &linked));
    QVERIFY(linked.attributes & FILE_ATTRIBUTE_REPARSE_POINT);
    QCOMPARE(linked.tag, DWORD(IO_REPARSE_TAG_MOUNT_POINT));
    QVERIFY(candidateRejectReparse(linked));

    // A future plaintext file behind a redirected *parent* is not safe.
    QVERIFY(!inspectParentComponentsNoFollow(
        redirect + QStringLiteral("/restored.txt")));

    // Detach junction without recursively traversing through it.
    QVERIFY(removeTemporaryJunction(redirect));
    QVERIFY(QDir().rmdir(redirect));
    QFile preserved(targetFile);
    QVERIFY(preserved.open(QIODevice::ReadOnly));
    QCOMPARE(preserved.readAll(), sentinel);
  }

  void nativeRelativeIntermediateNtfsJunctionIsDenied() {
    // A real junction is the *intermediate* component of a hypothetical
    // approved/redirected-child/sentinel.txt restore destination. Only
    // synthetic temporary files are used. We never open the leaf via link.
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString approved = temporary.filePath(QStringLiteral("approved"));
    const QString target = temporary.filePath(QStringLiteral("outside-target"));
    const QString redirect = approved + QStringLiteral("/redirected-child");
    QVERIFY(QDir().mkpath(approved));
    QVERIFY(QDir().mkpath(target));
    QVERIFY(QDir().mkpath(redirect));
    const QByteArray sentinel("OUTSIDE TARGET - DO NOT FOLLOW");
    const QString targetFile = target + QStringLiteral("/sentinel.txt");
    QVERIFY(writePublicBytes(targetFile, sentinel));

    DWORD error = ERROR_SUCCESS;
    QVERIFY2(createTemporaryNtfsJunction(redirect, target, &error),
             qPrintable(QStringLiteral("Mandatory NTFS junction fixture failed, Win32 %1")
                 .arg(error)));

    // Pin approved root and inspect the actual opened object, not a
    // pathname lookup or a QFileInfo-derived parent chain.
    UniqueHandle root(openNoFollow(approved, FILE_TRAVERSE |
                                              FILE_READ_ATTRIBUTES | SYNCHRONIZE));
    QVERIFY2(root.valid(), "Cannot open synthetic approved directory");
    ObservedObject rootInfo{};
    QVERIFY(inspectOpened(root.get(), &rootInfo));
    QVERIFY(!(rootInfo.attributes & FILE_ATTRIBUTE_REPARSE_POINT));

    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    QVERIFY(ntdll != nullptr);
    using NtCreateFileFn = NTSTATUS (NTAPI *)(PHANDLE, ACCESS_MASK,
        POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER,
        ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
    auto ntCreateFile = reinterpret_cast<NtCreateFileFn>(
        GetProcAddress(ntdll, "NtCreateFile"));
    QVERIFY2(ntCreateFile != nullptr, "NtCreateFile not available");

    wchar_t component[] = L"redirected-child"; // A single trusted fixture component.
    UNICODE_STRING name{};
    name.Buffer = component;
    name.Length = USHORT((sizeof(component) / sizeof(wchar_t) - 1) * sizeof(wchar_t));
    name.MaximumLength = USHORT(sizeof(component));
    OBJECT_ATTRIBUTES attrs{};
    attrs.Length = sizeof(attrs);
    attrs.RootDirectory = root.get();
    attrs.ObjectName = &name;
    attrs.Attributes = OBJ_CASE_INSENSITIVE;
    IO_STATUS_BLOCK ios{};
    HANDLE raw = INVALID_HANDLE_VALUE;
    constexpr ULONG kOpenExisting = 1;
    constexpr ULONG kDirectoryOnly = 0x00000001;
    constexpr ULONG kSynchronous = 0x00000020;
    constexpr ULONG kOpenReparseItself = 0x00200000;
    const NTSTATUS status = ntCreateFile(&raw,
        FILE_READ_ATTRIBUTES | FILE_TRAVERSE | SYNCHRONIZE,
        &attrs, &ios, nullptr, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        kOpenExisting, kDirectoryOnly | kSynchronous | kOpenReparseItself,
        nullptr, 0);
    {
      UniqueHandle child(raw);
      QVERIFY2(status >= 0 && child.valid(),
               qPrintable(QStringLiteral("Native no-follow junction open failed: NTSTATUS 0x%1")
                 .arg(quint32(status), 8, 16, QLatin1Char('0'))));
      ObservedObject observed{};
      QVERIFY(inspectOpened(child.get(), &observed));
      QVERIFY(observed.attributes & FILE_ATTRIBUTE_REPARSE_POINT);
      QCOMPARE(observed.tag, DWORD(IO_REPARSE_TAG_MOUNT_POINT));

      // Fail closed: a restore walker MUST stop here and never open
      // the hypothetical sentinel.txt under this intermediate component.
      QVERIFY(candidateRejectReparse(observed));
    }

    QVERIFY(removeTemporaryJunction(redirect));
    QVERIFY(QDir().rmdir(redirect));
    QFile preserved(targetFile);
    QVERIFY(preserved.open(QIODevice::ReadOnly));
    QCOMPARE(preserved.readAll(), sentinel);
    // This is a deterministic research fixture, NOT a complete secure
    // directory resolver, write/commit algorithm or TOCTOU proof.
  }

  void openedHandleIdentitySurvivesPathReplacement() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString name = root.filePath(QStringLiteral("stable-name.dat"));
    const QString renamed = root.filePath(QStringLiteral("moved-away.dat"));
    QVERIFY(writePublicBytes(name, "ORIGINAL PUBLIC FILE"));

    // Keep the real object open. A pathname is only a lookup key; it is
    // not a stable identity once another process can change the namespace.
    UniqueHandle pinned(openNoFollow(name, FILE_READ_ATTRIBUTES));
    QVERIFY(pinned.valid());
    ObservedObject before{}, stillPinned{}, replacement{};
    QVERIFY(inspectOpened(pinned.get(), &before));
    QVERIFY(MoveFileExW(
        reinterpret_cast<LPCWSTR>(name.utf16()),
        reinterpret_cast<LPCWSTR>(renamed.utf16()), 0));
    QVERIFY(writePublicBytes(name, "DIFFERENT PUBLIC FILE"));
    QVERIFY(inspectOpened(pinned.get(), &stillPinned));
    QVERIFY(inspectPathNoFollow(name, &replacement));
    QVERIFY(sameOpenedIdentity(before, stillPinned));
    QVERIFY(!sameOpenedIdentity(before, replacement));
    QFile original(renamed), newFile(name);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), QByteArray("ORIGINAL PUBLIC FILE"));
    QVERIFY(newFile.open(QIODevice::ReadOnly));
    QCOMPARE(newFile.readAll(), QByteArray("DIFFERENT PUBLIC FILE"));
  }
};
QTEST_GUILESS_MAIN(TestPortableWindowsReparse)
#include "test_portable_windows_reparse.moc"

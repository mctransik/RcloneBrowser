// Windows-only synthetic native RootDirectory research. No application linkage.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>
#include <QtTest>
#include <QDir>
#include <QFile>
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

    QVERIFY2(root.rename("approved", "approved-moved"),
             "Could not rename synthetic ancestor while handles are open");
    QVERIFY(root.mkpath("approved/nested"));
    QVERIFY(writeFixture(QDir(root.filePath("approved/nested")).filePath("marker.txt"),
                         "REPLACEMENT-NESTED"));

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

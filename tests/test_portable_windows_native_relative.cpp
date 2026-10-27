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

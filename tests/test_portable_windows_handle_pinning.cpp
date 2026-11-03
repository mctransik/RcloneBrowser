// ISOLATED WINDOWS RESEARCH: opened directory identity under path replacement.
// Not handle-relative secure restore, not runtime code, never package this test.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace {
class Handle {
 public:
  explicit Handle(HANDLE h): h_(h) {}
  ~Handle() { if (h_ != INVALID_HANDLE_VALUE && h_ != nullptr) CloseHandle(h_); }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  HANDLE get() const { return h_; }
  bool valid() const { return h_ != INVALID_HANDLE_VALUE && h_ != nullptr; }
 private:
  HANDLE h_;
};
HANDLE openDirectory(const QString& path) {
  return CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()),
      FILE_READ_ATTRIBUTES | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE |
      FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
}
bool identity(HANDLE h, BY_HANDLE_FILE_INFORMATION* info) {
  return GetFileInformationByHandle(h, info) != FALSE;
}
bool equalIdentity(const BY_HANDLE_FILE_INFORMATION& a,
                   const BY_HANDLE_FILE_INFORMATION& b) {
  return a.dwVolumeSerialNumber == b.dwVolumeSerialNumber &&
         a.nFileIndexHigh == b.nFileIndexHigh &&
         a.nFileIndexLow == b.nFileIndexLow;
}
}
class WindowsHandlePinningResearch : public QObject {
  Q_OBJECT
 private slots:
  void openedDirectoryDoesNotBecomeReplacement() {
    QTemporaryDir temp;
    QVERIFY2(temp.isValid(), "Temporary directory unavailable");
    QDir root(temp.path());
    QVERIFY(root.mkdir("approved"));
    const QString original = root.filePath("approved");
    Handle pinned(openDirectory(original));
    QVERIFY2(pinned.valid(), "Cannot open disposable directory handle");
    BY_HANDLE_FILE_INFORMATION before{}, after{}, replacement{};
    QVERIFY(identity(pinned.get(), &before));
    QVERIFY2(root.rename("approved", "approved-renamed"),
             "Cannot rename disposable directory while handle remains open");
    QVERIFY(root.mkdir("approved"));
    Handle other(openDirectory(original));
    QVERIFY(other.valid());
    QVERIFY(identity(pinned.get(), &after));
    QVERIFY(identity(other.get(), &replacement));
    QVERIFY(equalIdentity(before, after));
    QVERIFY2(!equalIdentity(before, replacement),
             "Different directories unexpectedly share a file identity");
    // The test intentionally never writes through a path or native handle.
    // A pinned handle alone does NOT implement race-free relative traversal.
  }
};
QTEST_MAIN(WindowsHandlePinningResearch)
#include "test_portable_windows_handle_pinning.moc"

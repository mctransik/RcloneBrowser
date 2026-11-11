#include "task_list_transaction.h"

#include <QtTest>

class TestTaskListTransaction : public QObject {
  Q_OBJECT

private slots:
  void failedNewTaskSaveRollsBackAppend() {
    int first = 1, second = 2;
    QList<int *> tasks{&first};
    int calls = 0;
    int sizeSeenBySave = 0;
    QVERIFY(!TaskListTransaction::persist(tasks, &second, [&] {
      ++calls;
      sizeSeenBySave = tasks.size(); // serializer sees the proposed list
      return false;
    }));
    QCOMPARE(calls, 1);
    QCOMPARE(sizeSeenBySave, 2);
    QCOMPARE(tasks.size(), 1);
    QCOMPARE(tasks.at(0), &first);
  }

  void successfulNewTaskSaveKeepsAppend() {
    int first = 1, second = 2;
    QList<int *> tasks{&first};
    QVERIFY(TaskListTransaction::persist(
        tasks, &second, [] { return true; }));
    QCOMPARE(tasks.size(), 2);
    QCOMPARE(tasks.at(1), &second);
  }

  void failedExistingTaskSaveDoesNotDuplicateItem() {
    int item = 1;
    QList<int *> tasks{&item};
    QVERIFY(!TaskListTransaction::persist(
        tasks, &item, [] { return false; }));
    QCOMPARE(tasks.size(), 1);
    QCOMPARE(tasks.at(0), &item);
  }

  void failedDeletionRestoresOriginalOrdering() {
    int first = 1, second = 2, third = 3;
    QList<int *> tasks{&first, &second, &third};
    int sizeSeenBySave = 0;
    QVERIFY(!TaskListTransaction::forget(tasks, &second, [&] {
      sizeSeenBySave = tasks.size(); // serializer sees the proposed deletion
      return false;
    }));
    QCOMPARE(sizeSeenBySave, 2);
    QCOMPARE(tasks.size(), 3);
    QCOMPARE(tasks.at(0), &first);
    QCOMPARE(tasks.at(1), &second);
    QCOMPARE(tasks.at(2), &third);
  }

  void successfulDeletionPersistsListChange() {
    int first = 1, second = 2;
    QList<int *> tasks{&first, &second};
    QVERIFY(TaskListTransaction::forget(
        tasks, &first, [] { return true; }));
    QCOMPARE(tasks.size(), 1);
    QCOMPARE(tasks.at(0), &second);
  }

  void unknownDeletionDoesNotAttemptDiskWrite() {
    int first = 1, missing = 2;
    QList<int *> tasks{&first};
    int calls = 0;
    QVERIFY(!TaskListTransaction::forget(tasks, &missing, [&] {
      ++calls;
      return true;
    }));
    QCOMPARE(calls, 0);
    QCOMPARE(tasks.size(), 1);
  }

  void nullItemsAreRejectedWithoutDiskWrite() {
    QList<int *> tasks;
    int calls = 0;
    auto save = [&] {
      ++calls;
      return true;
    };
    QVERIFY(!TaskListTransaction::persist(tasks, static_cast<int *>(nullptr), save));
    QVERIFY(!TaskListTransaction::forget(tasks, static_cast<int *>(nullptr), save));
    QCOMPARE(calls, 0);
    QVERIFY(tasks.isEmpty());
  }
};

QTEST_GUILESS_MAIN(TestTaskListTransaction)
#include "test_task_list_transaction.moc"

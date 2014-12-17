library sqlite;

import 'dart:async';
import 'dart:isolate';
import 'dart:collection';
import 'dart-ext:sqlite';

SendPort newServicePort() native "SQLiteServicePort";

class SQLiteError extends Error {
  final int status;
  final String message;
  SQLiteError(int this.status, String this.message);
}


class CallInfo {
  static int _callInfoID = 0;

  final String method;
  final List args;
  final bool checkError;
  final int batchID;
  final Completer completer;
  final int callInfoID;

  CallInfo(String this.method, List this.args, bool this.checkError, int this.batchID) : completer = new Completer(), callInfoID = ++_callInfoID {}
}

class SQLite {
  SendPort servicePort;
  RawReceivePort replyPort;
  Queue<CallInfo> sentCallInfos;
  LinkedHashSet<CallInfo> queuedCallInfos;

  int currentBatchID;
  static int _batchID = 0;

  SQLite() : servicePort = newServicePort(), replyPort = new RawReceivePort(), sentCallInfos = new Queue<CallInfo>(), queuedCallInfos = new LinkedHashSet<CallInfo>(), currentBatchID = null {
    replyPort.handler = (List result) {
      CallInfo callInfo = sentCallInfos.removeFirst();
      if (!callInfo.checkError || result[1] == SQLITE_OK || result[1] == SQLITE_ROW || result[1] == SQLITE_DONE)
        callInfo.completer.complete(result.sublist(1));
      else {
        print("SQLite call ${callInfo.method} failed error: ${result[1]}");
        callInfo.completer.completeError(new SQLiteError(result[1], "SQLite call ${callInfo.method} failed error: ${result[1]}"));
      }
    };
  }

  Future<List> _queueCall(String method, List args, bool checkError, int batchID) {
    CallInfo callInfo = new CallInfo(method, args, checkError, batchID);
    queuedCallInfos.add(callInfo);
    _processQueue();
    return callInfo.completer.future;
  } 

  void _processQueue() {
    List removeList = [];
    for (CallInfo callInfo in queuedCallInfos) {
      if (currentBatchID == null || currentBatchID == callInfo.batchID) {
        currentBatchID = callInfo.batchID;
        removeList.add(callInfo);
        sentCallInfos.addLast(callInfo);
        List l = [replyPort.sendPort, callInfo.method, callInfo.callInfoID];
        l.addAll(callInfo.args);
        servicePort.send(l);
      }
    }
    queuedCallInfos.removeAll(removeList);
  }

  int beginBatch() => ++_batchID;

  int endBatch() {
    currentBatchID = null;
    _processQueue();
  }

  Future<List> config({int log: 0, bool checkError: true, int batchID: null}) => _queueCall('config', [log], checkError, batchID);

  Future<List> executeNonSelect(int db, dynamic sql, {List params: null, bool checkError: true, int batchID: null}) => _queueCall("executeNonSelect", [db, sql, params], checkError, batchID);

  Future<List> executeSelect(int db, dynamic sql, {List params: null, bool checkError: true, int batchID: null}) => _queueCall("executeSelect", [db, sql, params], checkError, batchID);

  Future<List> prepare(int db, String sql, {bool checkError: true, int batchID: null}) => _queueCall("prepare", [db, sql], checkError, batchID);

  Future<List> finalize(int db, int statement, {bool checkError: true, int batchID: null}) => _queueCall("finalize", [db, statement], checkError, batchID);

  Future<List> open(String fileName, int flags, {bool checkError: true, int batchID: null}) => _queueCall("open", [fileName, flags], checkError, batchID);

  Future<List> close(int db, {bool checkError: true, int batchID: null}) => _queueCall("close", [db], checkError, batchID);

  Future<List> busyTimeout(int db, int ms, {bool checkError: true, int batchID: null}) => _queueCall("busyTimeout", [db, ms], checkError, batchID);

  void terminate() => replyPort.close();

}

const int SQLITE_OK = 0;   /* Successful result */
const int SQLITE_ERROR = 1;   /* SQL error or missing database */
const int SQLITE_INTERNAL = 2;   /* Internal logic error in SQLite */
const int SQLITE_PERM = 3;   /* Access permission denied */
const int SQLITE_ABORT = 4;   /* Callback routine requested an abort */
const int SQLITE_BUSY = 5;   /* The database file is locked */
const int SQLITE_LOCKED = 6;   /* A table in the database is locked */
const int SQLITE_NOMEM = 7;   /* A malloc() failed */
const int SQLITE_READONLY = 8;   /* Attempt to write a readonly database */
const int SQLITE_INTERRUPT = 9;   /* Operation terminated by sqlite3_interrupt()*/
const int SQLITE_IOERR = 10;   /* Some kind of disk I/O error occurred */
const int SQLITE_CORRUPT = 11;   /* The database disk image is malformed */
const int SQLITE_NOTFOUND = 12;   /* Unknown opcode in sqlite3_file_control() */
const int SQLITE_FULL = 13;   /* Insertion failed because database is full */
const int SQLITE_CANTOPEN = 14;   /* Unable to open the database file */
const int SQLITE_PROTOCOL = 15;   /* Database lock protocol error */
const int SQLITE_EMPTY = 16;   /* Database is empty */
const int SQLITE_SCHEMA = 17;   /* The database schema changed */
const int SQLITE_TOOBIG = 18;   /* String or BLOB exceeds size limit */
const int SQLITE_CONSTRAINT = 19;   /* Abort due to constraint violation */
const int SQLITE_MISMATCH = 20;   /* Data type mismatch */
const int SQLITE_MISUSE = 21;   /* Library used incorrectly */
const int SQLITE_NOLFS = 22;   /* Uses OS features not supported on host */
const int SQLITE_AUTH = 23;   /* Authorization denied */
const int SQLITE_FORMAT = 24;   /* Auxiliary database format error */
const int SQLITE_RANGE = 25;   /* 2nd parameter to sqlite3_bind out of range */
const int SQLITE_NOTADB = 26;   /* File opened that is not a database file */
const int SQLITE_NOTICE = 27;   /* Notifications from sqlite3_log() */
const int SQLITE_WARNING = 28;   /* Warnings from sqlite3_log() */
const int SQLITE_ROW = 100;  /* sqlite3_step() has another row ready */
const int SQLITE_DONE = 101;  /* sqlite3_step() has finished executing */

const int SQLITE_OPEN_READONLY = 0x00000001;  /* Ok for sqlite3_open_v2() */
const int SQLITE_OPEN_READWRITE = 0x00000002;  /* Ok for sqlite3_open_v2() */
const int SQLITE_OPEN_CREATE = 0x00000004;  /* Ok for sqlite3_open_v2() */
const int SQLITE_OPEN_DELETEONCLOSE = 0x00000008;  /* VFS only */
const int SQLITE_OPEN_EXCLUSIVE = 0x00000010;  /* VFS only */
const int SQLITE_OPEN_AUTOPROXY = 0x00000020;  /* VFS only */
const int SQLITE_OPEN_URI = 0x00000040;  /* Ok for sqlite3_open_v2() */
const int SQLITE_OPEN_MEMORY = 0x00000080;  /* Ok for sqlite3_open_v2() */
const int SQLITE_OPEN_MAIN_DB = 0x00000100;  /* VFS only */
const int SQLITE_OPEN_TEMP_DB = 0x00000200;  /* VFS only */
const int SQLITE_OPEN_TRANSIENT_DB = 0x00000400;  /* VFS only */
const int SQLITE_OPEN_MAIN_JOURNAL = 0x00000800;  /* VFS only */
const int SQLITE_OPEN_TEMP_JOURNAL = 0x00001000;  /* VFS only */
const int SQLITE_OPEN_SUBJOURNAL = 0x00002000;  /* VFS only */
const int SQLITE_OPEN_MASTER_JOURNAL = 0x00004000;  /* VFS only */
const int SQLITE_OPEN_NOMUTEX = 0x00008000;  /* Ok for sqlite3_open_v2() */
const int SQLITE_OPEN_FULLMUTEX = 0x00010000;  /* Ok for sqlite3_open_v2() */
const int SQLITE_OPEN_SHAREDCACHE = 0x00020000;  /* Ok for sqlite3_open_v2() */
const int SQLITE_OPEN_PRIVATECACHE = 0x00040000;  /* Ok for sqlite3_open_v2() */
const int SQLITE_OPEN_WAL = 0x00080000;  /* VFS only */

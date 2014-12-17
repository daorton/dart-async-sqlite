#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "sqlite3.h"
#include "include/dart_api.h"
#include "include/dart_native_api.h"

static int log = 0;

enum ARGS {SENDPORT = 0, METHOD, ID, ARG1, ARG2, ARG3, ARG4};

struct Message {
  Dart_CObject* m;
  Message(Dart_CObject* m) : m(m) {}
  void get(int n, char*& string)  { string = m->value.as_array.values[n]->value.as_string; }
  void get(int n, int32_t& int32) { int32  = m->value.as_array.values[n]->value.as_int32; }
  void get(int n, int64_t& int64) { int64  = m->value.as_array.values[n]->value.as_int64; }
  void get(int n, double& d)      { d      = m->value.as_array.values[n]->value.as_double; }
  void get(int n, void*& ptr)     { ptr    = (void*) m->value.as_array.values[n]->value.as_int64; }
};

struct Result {
  Dart_CObject** results;
  int nResults;
  int capacity;
  Result() : results(NULL), nResults(0), capacity(0) {}
  ~Result() {
    for (int i = 0; i < nResults; ++i)
      doDelete(results[i]);
    delete [] results;

  }

  void doDelete(Dart_CObject* r) {
    if (r->type == Dart_CObject_kString) {
      delete [] r->value.as_string;
    } else if (r->type == Dart_CObject_kArray) {
      for (int i = 0; i < r->value.as_array.length; ++i)
        doDelete(r->value.as_array.values[i]);
    }
    delete r;
  }

  void reserve(int newCapacity) {
    if(newCapacity > nResults) {
      Dart_CObject** oldResults = results;
      results = new Dart_CObject*[newCapacity];
      for(int k = 0; k < nResults; ++k)
        results[k] = oldResults[k];
      capacity = newCapacity;
      delete [] oldResults;
    }
  }

  void addResult(Dart_CObject* r) {
    if (nResults == capacity)
      reserve(2 * capacity);
    results[nResults++] = r;
  }

  void add(int32_t value) {
    Dart_CObject* r = new Dart_CObject();
    r->type = Dart_CObject_kInt32;
    r->value.as_int32 = value;
    addResult(r);
  }

  void add(int64_t value) {
    Dart_CObject* r = new Dart_CObject();
    r->type = Dart_CObject_kInt64;
    r->value.as_int64 = value;
    addResult(r);
  }

  void add(double value) {
    Dart_CObject* r = new Dart_CObject();
    r->type = Dart_CObject_kDouble;
    r->value.as_double = value;
    addResult(r);
  }

  void add(void* value) {
    Dart_CObject* r = new Dart_CObject();
    r->type = Dart_CObject_kInt64;
    r->value.as_int64 = (int64_t) value;
    addResult(r);
  }

  void add(const char* value) {
    Dart_CObject* r = new Dart_CObject();
    char* s = new char[strlen(value) + 1];
    strcpy(s, value);
    r->type = Dart_CObject_kString;
    r->value.as_string = s;
    addResult(r);
  }

  void addNULL() {
    Dart_CObject* r = new Dart_CObject();
    r->type = Dart_CObject_kNull;
    addResult(r);
  }

  void add(Result& otherResult) { // this extracts array so we can delete otherResult
    Dart_CObject* r = new Dart_CObject();
    r->type = Dart_CObject_kArray;
    r->value.as_array.values = otherResult.results;
    r->value.as_array.length = otherResult.nResults;
    otherResult.results = NULL;
    otherResult.nResults = 0;
    otherResult.capacity = 0;
    addResult(r);
  }

};

void config(Message& message, Result& result) {
  int32_t l;
  message.get(ARG1, l);
  log = l;
  result.add(0);
  result.add(log);
}

void open(Message& message, Result& result) {
  char* fileName;
  message.get(ARG1, fileName);
  int32_t flags;
  message.get(ARG2, flags);

  if (log > 0) fprintf(stderr, "open: %s 0x%8.8x\n", fileName, flags);

  sqlite3* db;
  int error = sqlite3_open_v2(fileName, &db, flags, NULL);
  result.add(error);
  result.add(db);
}

void close(Message& message, Result& result) {
  sqlite3* db;
  message.get(ARG1, (void*&) db);

  if (log > 0) fprintf(stderr, "close\n");

  int error = sqlite3_close(db);

  result.add(error);
}

void busyTimeout(Message& message, Result& result) {
  sqlite3* db;
  message.get(ARG1, (void*&) db);
  int32_t ms;
  message.get(ARG2, ms);

  if (log > 0) fprintf(stderr, "busyTimeout: %d\n", ms);

  int error = sqlite3_busy_timeout(db, ms);

  result.add(error);
}

void prepare(Message& message, Result& result) {
  sqlite3* db;
  message.get(ARG1, (void*&) db);
  char* sql;
  message.get(ARG2, sql);

  if (log > 0) fprintf(stderr, "prepare: %s\n", sql);

  sqlite3_stmt* statement = NULL;
  const char* tail = NULL;

  int error = sqlite3_prepare_v2(db, sql, strlen(sql), &statement, &tail);

  result.add(error);
  result.add(statement);
}

void finalize(Message& message, Result& result) {
  sqlite3* db;
  message.get(ARG1, (void*&) db);
  sqlite3_stmt* statement;
  message.get(ARG2, (void*&) statement);

  if (log > 0) fprintf(stderr, "finalize: %p\n", statement);

  int error = sqlite3_finalize(statement);

  result.add(error);
}

void columnValues(sqlite3_stmt* statement, Result& result) {
  int count = sqlite3_column_count(statement);
  result.reserve(count);
  for (int i = 0; i < count; ++i) {
    switch (sqlite3_column_type(statement, i)) {
      case SQLITE_INTEGER:
        result.add((int64_t) sqlite3_column_int64(statement, i));
        break;
      case SQLITE_FLOAT:
        result.add(sqlite3_column_double(statement, i));
        break;
      case SQLITE_TEXT:
        result.add((const char *) sqlite3_column_text(statement, i));
        break;
      case SQLITE_NULL:
      default:
        result.addNULL();
        break;
   }
  }
}

int bind(sqlite3_stmt* statement, int length, Dart_CObject** values) {
  int error = SQLITE_OK;
  for (int i = 0; i < length && error == SQLITE_OK; ++i) {
    Dart_CObject* object = values[i];
    switch (object->type) {
      case Dart_CObject_kInt32:
        error = sqlite3_bind_int(statement, i + 1, object->value.as_int32);
        break;
      case Dart_CObject_kInt64:
        error = sqlite3_bind_int64(statement, i + 1, object->value.as_int64);
        break;
      case Dart_CObject_kDouble:
        error = sqlite3_bind_double(statement, i + 1, object->value.as_double);
        break;
      case Dart_CObject_kString:
        error = sqlite3_bind_text(statement, i + 1, object->value.as_string, -1, SQLITE_TRANSIENT);
        break;
      case Dart_CObject_kNull:
        error = sqlite3_bind_null(statement, i + 1);
        break;
    }
  }
  return error;
}

void executeNonSelect(Message& message, Result& result) {
  sqlite3* db;
  message.get(ARG1, (void*&) db);

  char* sql = NULL;
  sqlite3_stmt* statement = NULL;
  if (message.m->value.as_array.values[ARG2]->type == Dart_CObject_kString)
    message.get(ARG2, sql);
  else
    message.get(ARG2, (void*&) statement);

  int length = message.m->value.as_array.values[ARG3]->type == Dart_CObject_kArray ? (int) message.m->value.as_array.values[ARG3]->value.as_array.length : 0;
  Dart_CObject** values = length > 0 ? message.m->value.as_array.values[ARG3]->value.as_array.values : NULL;

  if (log > 0) {
    if (sql)
      fprintf(stderr, "executeNonSelect: %s #params: %d\n", sql, length);
    else
      fprintf(stderr, "executeNonSelect: %p #params: %d\n", statement, length);
  }

  const char* tail = NULL;

  int error = sql == NULL ? SQLITE_OK : sqlite3_prepare_v2(db, sql, strlen(sql), &statement, &tail);
  if (error == SQLITE_OK) {
    if (length > 0)
      error = bind(statement, length, values);
    if (error == SQLITE_OK)
      error = sqlite3_step(statement);
    if (sql == NULL) {
      sqlite3_reset(statement);
      sqlite3_clear_bindings(statement);
    } else
      sqlite3_finalize(statement);
  }

  result.add(error);
  result.add((int64_t) sqlite3_last_insert_rowid(db));
  result.add(sqlite3_changes(db));
}

void executeSelect(Message& message, Result& result) {
  sqlite3* db;
  message.get(ARG1, (void*&) db);

  char* sql = NULL;
  sqlite3_stmt* statement = NULL;
  if (message.m->value.as_array.values[ARG2]->type == Dart_CObject_kString)
    message.get(ARG2, sql);
  else
    message.get(ARG2, (void*&) statement);

  int length = message.m->value.as_array.values[ARG3]->type == Dart_CObject_kArray ? (int) message.m->value.as_array.values[ARG3]->value.as_array.length : 0;
  Dart_CObject** values = length > 0 ? message.m->value.as_array.values[ARG3]->value.as_array.values : NULL;

  if (log > 0) {
    if (sql)
      fprintf(stderr, "executeSelect: %s\n", sql);
    else
      fprintf(stderr, "executeSelect: %p\n", statement);
  }

  Result rowData;
  rowData.reserve(100);
  const char* tail = NULL;
  int error = sql == NULL ? SQLITE_OK : sqlite3_prepare_v2(db, sql, strlen(sql), &statement, &tail);
  if (error == SQLITE_OK) {
    if (length > 0)
      error = bind(statement, length, values);
    if (error == SQLITE_OK) {
      error = sqlite3_step(statement);
      while (error == SQLITE_ROW) {
        Result r;
        columnValues(statement, r);
        rowData.add(r);
        error = sqlite3_step(statement);
      }
    }
    if (sql == NULL) {
      sqlite3_reset(statement);
      sqlite3_clear_bindings(statement);
    } else
      sqlite3_finalize(statement);
  }

  result.add(error);
  result.add(rowData);
}

void SQLiteService(Dart_Port dest_port_id, Dart_CObject* m) {
  Message message(m);
  char* method;
  message.get(METHOD, method);
  Result result;
  result.reserve(4);
  int32_t id;
  message.get(ID, id);
  result.add(id);

  if (log > 1) fprintf(stderr, "SQLiteService method: %s\n", method);

  if      (strcmp(method, "executeSelect")    == 0) executeSelect(message, result);
  else if (strcmp(method, "executeNonSelect") == 0) executeNonSelect(message, result);
  else if (strcmp(method, "prepare")          == 0) prepare(message, result);
  else if (strcmp(method, "finalize")         == 0) finalize(message, result);
  else if (strcmp(method, "open")             == 0) open(message, result);
  else if (strcmp(method, "close")            == 0) close(message, result);
  else if (strcmp(method, "busyTimeout")      == 0) busyTimeout(message, result);
  else if (strcmp(method, "config")           == 0) config(message, result);

  Dart_CObject r;
  r.type = Dart_CObject_kArray;
  r.value.as_array.length = result.nResults;
  r.value.as_array.values = result.results;
  Dart_Port sendPort;
  message.get(SENDPORT, sendPort);
  Dart_PostCObject(sendPort, &r);
}

Dart_Handle HandleError(Dart_Handle handle) {
  if (Dart_IsError(handle))
    Dart_PropagateError(handle);
  return handle;
}

void SQLiteServicePort(Dart_NativeArguments arguments) {
  Dart_EnterScope();
  Dart_SetReturnValue(arguments, Dart_Null());
  Dart_Port service_port = Dart_NewNativePort("SQLiteService", SQLiteService, true);
  if (service_port != ILLEGAL_PORT) {
    Dart_Handle send_port = HandleError(Dart_NewSendPort(service_port));
    Dart_SetReturnValue(arguments, send_port);
  }

  if (log > 1) fprintf(stderr, "**** SQLiteServicePort\n");

  Dart_ExitScope();
}

Dart_NativeFunction ResolveName(Dart_Handle name, int argc, bool* auto_setup_scope) {
  if (!Dart_IsString(name))
    return NULL;
  Dart_EnterScope();
  const char* cname;
  HandleError(Dart_StringToCString(name, &cname));
  Dart_NativeFunction result = NULL;
  if (strcmp(cname, "SQLiteServicePort") == 0) result = SQLiteServicePort;
  Dart_ExitScope();
  *auto_setup_scope = false;
  return result;
}

const uint8_t* ResolveSymbol(Dart_NativeFunction nf) {
  return NULL;
}

DART_EXPORT Dart_Handle sqlite_Init(Dart_Handle parent_library) {
  if (Dart_IsError(parent_library))
    return parent_library;
  Dart_Handle result_code = Dart_SetNativeResolver(parent_library, ResolveName, ResolveSymbol);

  if (log > 1) fprintf(stderr, "sqlite_Init\n");

  if (Dart_IsError(result_code))
    return result_code;
   return Dart_Null();
}

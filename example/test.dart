library test;

import 'dart:async';
import 'dart:io';
import 'dart:math';
import 'package:sqlite/sqlite.dart';

class Database {

  SQLite _sqlite;
  int _db;
  bool _prepared;

  Map<String, dynamic> preparedStatements = { // note that map String values are overwritten by statement int in prepare
    'CREATEUSER':           'INSERT INTO users (userName,password,points) values (?,?,NULL)',
    'GETUSER':              'SELECT userID,userName,points FROM users WHERE userID=?',
    'GETUSERS':             'SELECT userID,userName,points FROM users',
    'UPDATEUSERPOINTSNULL': 'UPDATE users SET points=NULL',
    'UPDATEUSERPOINTS':     'UPDATE users SET points=? WHERE userID=?',
  };

  Database(int log) {
    _sqlite = new SQLite()
      ..config(log: log);
    _prepared = false;
  }

  Future<int> open(String path, {bool create: false}) {
    return _sqlite.open(path, create ? SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE : SQLITE_OPEN_READWRITE)
      .then((result) {
        _db = result[1];
        _sqlite.busyTimeout(_db, 5000);
        return _db;
      });
  }

  Future<int> close() {
    return Future.forEach(_prepared ? preparedStatements.values : [], (int statement) => _sqlite.finalize(_db, statement)).then((_) =>
      _sqlite.close(_db).then((List result) {
        _sqlite.terminate();
        return result[0];
      }));
  }

  Future prepare() {
    _prepared = true;
    return Future.forEach(preparedStatements.keys, (key) =>
      _sqlite.prepare(_db, preparedStatements[key]).then((List result) => preparedStatements[key] = result[1]));
  }

  Future<int> create() {
    return _sqlite.executeNonSelect(_db, 'CREATE TABLE users (userID INTEGER PRIMARY KEY,userName TEXT,password TEXT,points INTEGER)')
      .then((_) => createUser('Guest', new Random().nextInt((1 << 32) - 1).toString())) // random password
      .then((_) => _sqlite.executeNonSelect(_db, 'CREATE INDEX usersByName ON users (userName COLLATE NOCASE ASC)'))
      ;
  }

  Future<int> createUser(String userName, String password, {int batchID: null}) {
    //String dbPass = digestToString(new SHA1().update(UTF8.encode('sunny' + userName.toLowerCase() + password)).digest());
    return _sqlite.executeNonSelect(_db, preparedStatements['CREATEUSER'], params: [userName, password], batchID: batchID)
      .then((result) => result[1]);
  }

  Future<Map> getUser(int userID) {
    return _sqlite.executeSelect(_db, preparedStatements['GETUSER'], params: [userID]).then((result) {
      List rows = result[1];
      if (rows.length == 0)
        return null;
      List r = rows[0];
      return {'userID': r[0], 'userName': r[1], 'points': r[2]};
    });
  }

  Future<List> getUsers() {
    return _sqlite.executeSelect(_db, preparedStatements['GETUSERS']).then((result) => result[1]);
  }

  Future createUsers(int nUserStart, int nUsers, {int batchID: null}) {
    int n = 0;
    return Future.doWhile(() => ++n == nUsers ? false : createUser('User${nUserStart + n - 1}', new Random().nextInt((1 << 32) - 1).toString(), batchID: batchID) != 0);
  }

  Future createUsersBatch(int nUserStart, int nUsers) {
    int batchID = _sqlite.beginBatch();
    return _sqlite.executeNonSelect(_db, 'BEGIN', batchID: batchID)
      .then((_) => createUsers(nUserStart, nUsers, batchID: batchID))
      .then((_) => _sqlite.executeNonSelect(_db, 'COMMIT', batchID: batchID))
      .then((_) => _sqlite.endBatch());
  }

}

main(List<String> arguments) {
  String dbPath = '/tmp/test.sqlite';
  new File(dbPath).deleteSync();
  int nUsers = 5000;
  int log = nUsers <= 100 ? 1 : 0;

  Database database = new Database(log);
  database.open(dbPath, create: true)
    .then((_) => database.create())
    .then((_) => database.prepare())
    .then((_) => print('Starting createUsers'))
    .then((_) => database.createUsers(1, nUsers))
    .then((_) => database.getUser(nUsers - 1).then((Map user) => print('Without transaction: $user')))
    .then((_) => print('Starting createUsersBatch'))
    .then((_) => database.createUsersBatch(1 + nUsers, nUsers))
    .then((_) => database.getUser(2 * nUsers - 1).then((Map user) => print('With transaction: $user')))
    .then((_) => database.close())
    .then((_) => print('$dbPath closed'));
}
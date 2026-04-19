/*
 * Astra Programming Language
 * table.h — Hash table for variables, string interning, etc.
 */

#ifndef astra_table_h
#define astra_table_h

#include "common.h"
#include "value.h"

/* ── Hash table entry ────────────────────────────────────── */

typedef struct {
    ObjString* key;
    Value      value;
} Entry;

/* ── Hash table ──────────────────────────────────────────── */

typedef struct {
    int    count;
    int    capacity;
    Entry* entries;
} Table;

void  initTable(Table* table);
void  freeTable(Table* table);
bool  tableGet(Table* table, ObjString* key, Value* value);
bool  tableSet(Table* table, ObjString* key, Value value);
bool  tableDelete(Table* table, ObjString* key);
void  tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash);
void  tableRemoveWhite(Table* table);
void  markTable(Table* table);

#endif /* astra_table_h */

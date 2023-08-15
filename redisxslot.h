#ifndef REDISXSLOT_H
#define REDISXSLOT_H

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "RedisModulesSDK/redismodule.h"
#include "RedisModulesSDK/rmutil/sds.h"
#include "dep/dict.h"
#include "dep/list.h"
#include "dep/skiplist.h"
#include "dep/util.h"
#include "hiredis/hiredis.h"
#include "threadpool/thpool.h"

// define error
#define REDISXSLOT_ERRORMSG_SYNTAX "ERR syntax error"
#define REDISXSLOT_ERRORMSG_MGRT "ERR migrate error"
#define REDISXSLOT_ERRORMSG_DEL "ERR del error"

// define const
#define DEFAULT_HASH_SLOTS_MASK 0x000003ff
#define DEFAULT_HASH_SLOTS_SIZE (DEFAULT_HASH_SLOTS_MASK + 1)
#define MAX_HASH_SLOTS_MASK 0x0000ffff
#define MAX_HASH_SLOTS_SIZE (MAX_HASH_SLOTS_MASK + 1)
#define MGRT_ONE_KEY_TIMEOUT 30  // 30s
#define REDIS_LONGSTR_SIZE 21    /* Bytes needed for long -> str */
#define SLOTS_MGRT_ERR -1
#define MAX_NUM_THREADS 128
#define REDISXSLOT_APIVER_1 1
/* Hash table parameters for resize */
#define HASHTABLE_MIN_FILL 10           /* Minimal hash table fill 10% */
#define HASHTABLE_MAX_LOAD_FACTOR 1.618 /* Maximum hash table load factor. */
/* sub generic cmd for evnet handle */
#define CMD_NONE 0
#define CMD_RENAME 1
#define CMD_MOVE 2

// define macro
#define UNUSED(V) ((void)V)
#define CREATE_CMD(name, tgt, attr, firstkey, lastkey, keystep)                \
    do {                                                                       \
        if (RedisModule_CreateCommand(ctx, name, tgt, attr, firstkey, lastkey, \
                                      keystep)                                 \
            != REDISMODULE_OK) {                                               \
            RedisModule_Log(ctx, REDISMODULE_LOGLEVEL_WARNING,                 \
                            "reg cmd error");                                  \
            return REDISMODULE_ERR;                                            \
        }                                                                      \
    } while (0);
#define CREATE_ROMCMD(name, tgt, firstkey, lastkey, keystep) \
    CREATE_CMD(name, tgt, "readonly", firstkey, lastkey, keystep);
#define CREATE_WRMCMD(name, tgt, firstkey, lastkey, keystep) \
    CREATE_CMD(name, tgt, "write deny-oom", firstkey, lastkey, keystep);
/* Using the following macro you can run code inside serverCron() with the
 * specified period, specified in milliseconds.
 * The actual resolution depends on server.hz. */
#define run_with_period(_ms_, _hz_) \
    if (((_ms_) <= 1000 / _hz_)     \
        || !(g_slots_meta_info.cronloops % ((_ms_) / (1000 / _hz_))))

// define struct type
typedef struct _slots_meta_info {
    uint32_t hash_slots_size;
    // from config databases
    int databases;
    // from config activerehashing yes(1)/no(0)
    int activerehashing;
    // cronloop event callback cn
    int cronloops;
    // thread pool size (mgrt restore)
    int slots_mgrt_threads;
    int slots_restore_threads;
} slots_meta_info;

typedef struct _db_slot_info {
    // current db
    int db;
    // rehash flag
    int slotkey_table_rehashing;
    // hash table entry: RedisModuleString* key,val(crc)
    dict** slotkey_tables;
    // member: RedisModuleString* key, score: uint32_t crc
    m_zskiplist* tagged_key_list;
} db_slot_info;

typedef struct _db_slot_mgrt_connet {
    int db;
    // todo: use `slotsmgrt.authset` cmd set host port pwd
    int authorized;
    time_t last_time;
    redisContext* conn_ctx;
} db_slot_mgrt_connect;

// declare struct and define diff type
struct _rdb_obj {
    RedisModuleString* key;
    RedisModuleString* val;
    time_t ttlms;
};
typedef struct _rdb_obj rdb_dump_obj;
typedef struct _rdb_obj rdb_parse_obj;

typedef struct _slots_restore_one_task_params {
    RedisModuleCtx* ctx;
    rdb_dump_obj* obj;
    int result_code;
} slots_restore_one_task_params;

// declare defined extern var to out use
extern slots_meta_info g_slots_meta_info;
extern db_slot_info* db_slot_infos;

// declare api function
void crc32_init();
uint32_t crc32_checksum(const char* buf, int len);
int slots_num(const char* s, uint32_t* pcrc, int* phastag);
void Slots_Init(RedisModuleCtx* ctx, uint32_t hash_slots_size, int databases,
                int num_threads, int activerehashing);
void Slots_Free(RedisModuleCtx* ctx);
int SlotsMGRT_OneKey(RedisModuleCtx* ctx, const char* host, const char* port,
                     time_t timeout, RedisModuleString* key,
                     const char* mgrtType);
int SlotsMGRT_SlotOneKey(RedisModuleCtx* ctx, const char* host,
                         const char* port, time_t timeout, int slot,
                         const char* mgrtType);
int SlotsMGRT_TagKeys(RedisModuleCtx* ctx, const char* host, const char* port,
                      time_t timeout, RedisModuleString* key,
                      const char* mgrtType);
int SlotsMGRT_TagSlotKeys(RedisModuleCtx* ctx, const char* host,
                          const char* port, time_t timeout, int slot,
                          const char* mgrtType);
int SlotsMGRT_Restore(RedisModuleCtx* ctx, rdb_dump_obj* objs[], int n);
void SlotsMGRT_Scan(RedisModuleCtx* ctx, int slot, unsigned long count,
                    unsigned long cursor, list* l);
int SlotsMGRT_DelSlotKeys(RedisModuleCtx* ctx, int db, int slots[], int n);
void SlotsMGRT_CloseTimedoutConns(RedisModuleCtx* ctx);
void Slots_Add(RedisModuleCtx* ctx, int db, RedisModuleString* key);
void Slots_Del(RedisModuleCtx* ctx, int db, RedisModuleString* key);

#endif /* REDISXSLOT_H */
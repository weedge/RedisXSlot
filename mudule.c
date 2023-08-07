/* slots migrate module -- batch dump rdb obj entries to migrate slots
 *
 * -----------------------------------------------------------------------------
 *
 * Copyright (c) 2023, weedge <weege007 at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "redismodule.h"
#include "redisxslot.h"

int SlotsHashKey_RedisCommand(RedisModuleCtx* ctx, RedisModuleString** argv,
                              int argc) {
    /* Use automatic memory management. */
    RedisModule_AutoMemory(ctx);

    if (argc < 2)
        return RedisModule_WrongArity(ctx);

    RedisModule_ReplyWithArray(ctx, argc - 1);
    for (int i = 1; i < argc; i++) {
        const char* key_ptr = RedisModule_StringPtrLen(argv[i], NULL);
        int slot = slots_num(key_ptr, NULL, NULL);
        RedisModule_Log(ctx, "debug", "s = %s slot = %d \n", key_ptr, slot);
        RedisModule_ReplyWithLongLong(ctx, slot);
    }

    return REDISMODULE_OK;
}

/* *
 * slotsinfo [start] [count]
 * */
int SlotsInfo_RedisCommand(RedisModuleCtx* ctx, RedisModuleString** argv,
                           int argc) {
    /* Use automatic memory management. */
    RedisModule_AutoMemory(ctx);

    if (argc >= 4)
        return RedisModule_WrongArity(ctx);

    long long start = 0;
    if (argc >= 2
        && RedisModule_StringToLongLong(argv[1], &start) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, REDISXSLOT_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    long long count = 0;
    if (argc >= 3
        && RedisModule_StringToLongLong(argv[2], &count) != REDISMODULE_OK) {
        RedisModule_ReplyWithError(ctx, REDISXSLOT_ERRORMSG_SYNTAX);
        return REDISMODULE_ERR;
    }

    int slots_slot[g_slots_meta_info.hash_slots_size];
    int slots_size[g_slots_meta_info.hash_slots_size];
    int n = 0;
    long long end = start + count;
    for (int i = start; i < end; i++) {
    }

    RedisModule_ReplyWithArray(ctx, n);
    for (int i = 0; i < n; i++) {
        RedisModule_ReplyWithArray(ctx, 2);
        RedisModule_ReplyWithLongLong(ctx, slots_slot[i]);
        RedisModule_ReplyWithLongLong(ctx, slots_size[i]);
    }

    return REDISMODULE_OK;
}

/* *
 * slotsmgrtone host port timeout key
 * */
int SlotsMGRTOne_RedisCommand(RedisModuleCtx* ctx, RedisModuleString** argv,
                              int argc) {
    return REDISMODULE_OK;
}

/* *
 * slotsmgrtslot host port timeout slot
 * */
int SlotsMGRTSlot_RedisCommand(RedisModuleCtx* ctx, RedisModuleString** argv,
                               int argc) {
    return REDISMODULE_OK;
}

/* *
 * slotsmgrttagone host port timeout key
 * */
int SlotsMGRTTagOne_RedisCommand(RedisModuleCtx* ctx, RedisModuleString** argv,
                                 int argc) {
    return REDISMODULE_OK;
}

/* *
 * slotsmgrttagslot host port timeout slot
 * */
int SlotsMGRTTagSlot_RedisCommand(RedisModuleCtx* ctx, RedisModuleString** argv,
                                  int argc) {
    return REDISMODULE_OK;
}

/* *
 * slotsdel slot1 [slot2 ...]
 * */
int SlotsDel_RedisCommand(RedisModuleCtx* ctx, RedisModuleString** argv,
                          int argc) {
    return REDISMODULE_OK;
}

/* *
 * slotsrestore key ttl val [key ttl val ...]
 * */
int SlotsRestore_RedisCommand(RedisModuleCtx* ctx, RedisModuleString** argv,
                              int argc) {
    return REDISMODULE_OK;
}

/* *
 * slotsscan slotnum cursor [COUNT count]
 * */
int SlotsScan_RedisCommand(RedisModuleCtx* ctx, RedisModuleString** argv,
                           int argc) {
    return REDISMODULE_OK;
}

/* This function must be present on each Redis module. It is used in order to
 * register the commands into the Redis server. */
int RedisModule_OnLoad(RedisModuleCtx* ctx, RedisModuleString** argv,
                       int argc) {
    if (RedisModule_Init(ctx, "redisxslot", 1, REDISMODULE_APIVER_1)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    // Log the list of parameters passing loading the module.
    for (int j = 0; j < argc; j++) {
        const char* s = RedisModule_StringPtrLen(argv[j], NULL);
        printf("ModuleLoaded with argv[%d] = %s\n", j, s);
    }

    // config get databases
    RedisModule_AutoMemory(ctx);
    RedisModuleCallReply* reply
        = RedisModule_Call(ctx, "CONFIG", "cc", "GET", "databases");
    long long items = RedisModule_CallReplyLength(reply);
    if (items != 2)
        return REDISMODULE_ERR;
    long long databases;
    RedisModule_StringToLongLong(
        RedisModule_CreateStringFromCallReply(
            RedisModule_CallReplyArrayElement(reply, 1)),
        &databases);

    // init
    long long hash_slots_size = DEFAULT_HASH_SLOTS_SIZE;
    if (argc >= 1
        && RedisModule_StringToLongLong(argv[0], &hash_slots_size)
               == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    if (hash_slots_size <= 0) {
        printf("[ERROR] ModuleLoaded hash slots size %lld <=0\n",
               hash_slots_size);
        return REDISMODULE_ERR;
    }
    if (hash_slots_size > MAX_HASH_SLOTS_SIZE) {
        printf("[ERROR] ModuleLoaded hash slots size %lld > max size %d\n",
               hash_slots_size, MAX_HASH_SLOTS_SIZE);
        return REDISMODULE_ERR;
    }
    slots_init(hash_slots_size, databases);

    if (RedisModule_CreateCommand(
            ctx, "slotshashkey", SlotsHashKey_RedisCommand, "readonly", 0, 0, 0)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "slotsinfo", SlotsInfo_RedisCommand,
                                  "readonly", 0, 0, 0)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(
            ctx, "slotsmgrtone", SlotsMGRTOne_RedisCommand, "readonly", 0, 0, 0)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "slotsmgrtslot",
                                  SlotsMGRTSlot_RedisCommand, "readonly", 0, 0,
                                  0)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "slotsmgrttagone",
                                  SlotsMGRTTagOne_RedisCommand, "readonly", 0,
                                  0, 0)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "slotsmgrttagslot",
                                  SlotsMGRTTagSlot_RedisCommand, "readonly", 0,
                                  0, 0)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "slotsrestore",
                                  SlotsRestore_RedisCommand, "write", 0, 0, 0)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "slotsdel", SlotsDel_RedisCommand,
                                  "write", 0, 0, 0)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    if (RedisModule_CreateCommand(ctx, "slotsscan", SlotsScan_RedisCommand,
                                  "write", 0, 0, 0)
        == REDISMODULE_ERR)
        return REDISMODULE_ERR;

    return REDISMODULE_OK;
}

int RedisModule_OnUnload(RedisModuleCtx* ctx) {
    REDISMODULE_NOT_USED(ctx);
    slots_free();
    return REDISMODULE_OK;
}

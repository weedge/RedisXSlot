[![licence](https://img.shields.io/github/license/weedge/redisxslot.svg)](https://github.com/weedge/redisxslot/blob/main/LICENSE)
[![release](https://img.shields.io/github/v/release/weedge/redisxslot.svg)](https://github.com/weedge/redisxslot/releases/latest)
# RedisxSlot

**Supported redis version**: redis >= 6.0
1. redis >= 5.0 (use module `dict/zset` api; if use 4.\*.\*, cp `dict/zset` op from redis do the same op; use `RedisModule_SubscribeToKeyspaceEvents` api sub keyspace events,see `RedisModuleEventCallback` api help doc detail)
2. redis >= 6.0 (use module `RedisModule_NotifyKeyspaceEvent` and Server events definitions `RedisModuleEvent_**` api, sub CronLoop(serverCron) event to do resize/rehash dict), `REDISMODULE_NOTIFY_LOADED` need load rdb init db slots keys.
3. redis >= 7.0 (use module `RedisModuleEvent_EVENTLOOP` api and use AE api with hiredis(1.2) adapter to connect)

**Tips**: use `ruby gendoc.rb | less` or `cat Module_Call | less` see api help doc or access [modules-api-ref](https://redis.io/docs/reference/modules/modules-api-ref/), hello** example and test/modules case.

# Feature
1. load module init hash slot size, default size 2^10, max size 2^16. (once make sure the slot size, don't change it)
2. load module init activerehashing,databases from config, activerehashing used to sub server event to rehash slot keys dict 
3. load module init num_threads, if thread_num>0,init thread pool size to do migrate job, default donot use thread pool.  
4. sub ServerEvent `CronLoop(ServerLoop),FlushDB,Shutdown`
    1. sub CronLoop server event hook to resize/rehash dict (db slot keys tables)
    2. sub FlushDB server event hook to delete one/all dict (db slot keys tables)
    3. sub Shutdown server event hook to release dicts (db slot keys tables) and free memory.
5. sub KeyspaceEvents `STRING,LIST,HASH,SET,ZSET, LOADED; GENERIC, EXPIRED`
    1. sub keyspaces `STRING,LIST,HASH,SET,ZSET, LOADED` notify event hook to add dict/skiplist keys
    2. sub keyspaces `GENERIC, EXPIRED` notify event hook to delete dict/skiplist keys
6. support slot tag key migrate, for (smart client/proxy)'s configSrv admin contoller layer use it.
    use `SLOTSMGRTTAGSLOT` cmd to migrate slot's key with same tag,
    default use slotsrestore batch send key, ttlms, dump rdb val ... (restore with replace)
7. `SLOTSRESTORE` if num_threads>0, init thread pool size to send `slotsrestore` batch keys job. loadmodule like this `./redis/src/redis-server --port 6379 --loadmodule ./redisxslot.so 1024 4 --dbfilename dump.6379.rdb`
8. about migrate cmd, create a thread async block todo per client, splite batch migrate, don't or less block other cmd run. loadmodule like this `./redis/src/redis-server --port 6379 --loadmodule ./redisxslot.so 1024 4 async --dbfilename dump.6379.rdb`
9. support setcpuaffinity for migrate async thread like redis bio job thread config setcpuaffinity on linux/bsd(syntax of cpu list looks like taskset).  loadmodule like this `./redis/src/redis-server --port 6379 --loadmodule ./redisxslot.so 1024 0 async 1,3 --dbfilename dump.6379.rdb` 
10. about migrate cmd, support pipeline buffer migrate, use migrate cmd like this `SLOTSMGRTTAGSLOT 127.0.0.1 6379 30000 835 withpipeline`. use `withpipeline` current don't support thread pool and async block migrate. 
# Build & LoadModule
```shell
git clone https://github.com/redis/redis.git
cd redis && make && cd ..
git clone https://github.com/weedge/redisxslot.git
# make help, default with hiredis static lib
cd redisxslot && make RM_INCLUDE_DIR=$(pwd)/../redis/src
../redis/src/redis-server --port 6379 --loadmodule ./redisxslot.so --dbfilename dump.6379.rdb
```
Tips: 
1. if want spec redis version(>=6.0.0) run `make RM_INCLUDE_DIR={redis_path}/src`,default use latest version(redismodule.h)
2. if use vscode debug, u can reference [docs/launch.json](./docs/launch.json)
# Release
use conanfile py script todo ci with makefile release.
# Test
1. use `redis/src/redis-benchmark` add test data, test 1m data mgrt, see [doc/test.md](./docs/test.md).
2. need use redis tcl script `test_helper.tcl` to run test case；run `bash -x tests/run_test.sh`, so easy~
3. ci loadmodule use test case to Redis test suite and run all test case. if new feat, add some test case in [tests/redisxslot.tcl](./tests/redisxslot.tcl)
# Docker
1. run in local docker
```shell
# build redisxslot with latest redis stable version 
make docker_img
# run redisxslot with latest redis stable version 
make docker_run
# then run redis-cli in container with bridge docker network for mgrt between container
```
2. if docker img is ok, u can push build's img to inner/cloud img hub or docker hub for ci/cd, like this:
```shell
# login
docker login
# tag a docker hub name
#docker image tag {taghash} weedge/redisxslot:latest_0.1.0
docker image tag redisxslot:latest_0.1.0 weedge/redisxslot:latest_0.1.0
# push your docker hub
docker push weedge/redisxslot:latest_0.1.0
# then pull this remote img to run, u can use it~ :)
docker run -itd --name weedge-redisxslot -p 16379:17000 weedge/redisxslot:latest_0.1.0
```
# Cmd Case
```shell
127.0.0.1:6660> setex 122{tag} 86400 v3
OK
127.0.0.1:6660> setex 1{tag} 86400 v3
OK
127.0.0.1:6660> sadd 12{tag} m1 m2 m3
(integer) 0
127.0.0.1:6660> hmset 123{tag} f1 v1 f2 v2
OK
127.0.0.1:6660> lpush 1234{tag} l1 l2
(integer) 2
127.0.0.1:6660> zadd 1235{tag} 100 z1 10 z2
(integer) 2
127.0.0.1:6660> slotshashkey 123{tag}
1) (integer) 899
127.0.0.1:6660> slotsinfo 899 1
1) 1) (integer) 899
   1) (integer) 6
127.0.0.1:6660> SLOTSMGRTTAGSLOT 127.0.0.1 6666 3000 899
1) (integer) 6
2) (integer) 0
127.0.0.1:6660> SLOTSMGRTTAGSLOT 127.0.0.1 6666 3000 899
1) (integer) 0
2) (integer) 0
```
```shell
127.0.0.1:6666> slotsinfo 0 1024
1) 1) (integer) 899
   1) (integer) 6
127.0.0.1:6666> get 122{tag}
"v3"
127.0.0.1:6666> ttl 122{tag}
(integer) 86133
127.0.0.1:6666> get 1{tag}
"v3"
127.0.0.1:6666> ttl 1{tag}
(integer) 86120
127.0.0.1:6666> hgetall 123{tag}
1) "f1"
2) "v1"
3) "f2"
4) "v2"
127.0.0.1:6666> lrange 1234{tag} 0 -1
1) "l2"
2) "l1"
127.0.0.1:6666> zrange 1235{tag} 0 10 withscores
1) "z2"
2) "10"
3) "z1"
4) "100"
```
Tips: 
1. if use codis-proxy, codis-dashboard config set `migration_method = "sync"`
2. <u>if use redisxslot module to migrate with slot, please use cmd `slotsrestore` cmd instead of `restore` to migrate<u>

# SuperMarioBros
1. [xdis-storager](https://github.com/weedge/xdis-storager)
2. [xdis-tikv](https://github.com/weedge/xdis-tikv)

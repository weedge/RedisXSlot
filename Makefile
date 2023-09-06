# ReidsXSlot Makefile
# Copyright (C) 2023- weedge <weege007 at gmail dot com>
# This file is released under the MIT license, see the LICENSE file

CC=gcc
BUILD_TYPE ?= Debug
# redis version >= 6.0.0
# or use RedisModule_GetServerVersion but version >= 6.0.9
REDIS_VERSION ?= 60000

# redisxslot version
REDISXSLOT_MAJOR=$(shell grep REDISXSLOT_MAJOR redisxslot.h | awk '{print $$3}')
REDISXSLOT_MINOR=$(shell grep REDISXSLOT_MINOR redisxslot.h | awk '{print $$3}')
REDISXSLOT_PATCH=$(shell grep REDISXSLOT_PATCH redisxslot.h | awk '{print $$3}')
REDISXSLOT_SONAME=$(shell grep REDISXSLOT_SONAME redisxslot.h | awk '{print $$3}')

# RedisModulesSDK
SDK_DIR = ${SOURCEDIR}/RedisModulesSDK
SDK_CFLAGS ?= -I$(SDK_DIR) -I$(SDK_DIR)/rmutil
SDK_LDFLAGS ?= -L$(SDK_DIR)/rmutil -lrmutil

# hiredis
HIREDIS_DIR = ${SOURCEDIR}/hiredis
HIREDIS_RUNTIME_DIR ?= $(SOURCEDIR)
HIREDIS_CFLAGS ?= -I$(HIREDIS_DIR) -I$(HIREDIS_DIR)/adapters
HIREDIS_LDFLAGS ?= -L$(HIREDIS_DIR)
HIREDIS_STLIB ?= $(HIREDIS_DIR)/libhiredis.a
ifeq ($(uname_S),Darwin)
HIREDIS_DYLIB ?= $(HIREDIS_LDFLAGS) -lhiredis -rpath $(HIREDIS_RUNTIME_DIR)
else
HIREDIS_DYLIB ?= $(HIREDIS_LDFLAGS) -lhiredis -rpath=$(HIREDIS_RUNTIME_DIR)
endif

HIREDIS_LIB_FLAGS ?= $(HIREDIS_LDFLAGS) $(HIREDIS_DIR)/libhiredis.a
ifeq ($(HIREDIS_USE_DYLIB),1)
HIREDIS_LIB_FLAGS = $(HIREDIS_DYLIB)
endif

# threadpool
THREADPOOL_DIR = ${SOURCEDIR}/threadpool
THREADPOOL_CFLAGS ?= -I$(THREADPOOL_DIR)

# dep
DEP_DIR = ${SOURCEDIR}/dep
DEP_CFLAGS ?= -I$(DEP_DIR)

#set environment variable RM_INCLUDE_DIR to the location of redismodule.h
ifndef RM_INCLUDE_DIR
	RM_INCLUDE_DIR=$(SDK_DIR)
endif

ENABLE_SANITIZE?=NO
SANITIZE_CFLAGS?=
SANITIZE_LDLAGS?=
OPTIMIZE_CFLAGS?=-O3
ifeq ($(BUILD_TYPE),Debug)
ifeq ($(ENABLE_SANITIZE),YES)
# https://gist.github.com/weedge/bdf786fb9ccdf4d84ba08ae8e71c5f98
# https://github.com/google/sanitizers/issues/679
	SANITIZE_CFLAGS=-fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-scope 
	SANITIZE_LDLAGS=-fsanitize=address -lasan
endif
	OPTIMIZE_CFLAGS=-O1
endif
# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?= $(OPTIMIZE_CFLAGS) \
					$(SANITIZE_CFLAGS) \
					-DREDIS_VERSION=$(REDIS_VERSION) -I$(RM_INCLUDE_DIR) \
					-fPIC -W -Wall -fno-common -g -ggdb -std=gnu99 \
					-D_GNU_SOURCE -D_XOPEN_SOURCE=600 \
					-pthread -fvisibility=hidden 
	SHOBJ_LDFLAGS ?= -fPIC -shared -Bsymbolic \
					$(SANITIZE_LDLAGS) \
					-fvisibility=hidden
					
else
	SHOBJ_CFLAGS ?= $(OPTIMIZE_CFLAGS) \
					$(SANITIZE_CFLAGS) \
					-DREDIS_VERSION=$(REDIS_VERSION) -I$(RM_INCLUDE_DIR) \
					-fPIC -W -Wall -dynamic -fno-common -g -ggdb -std=gnu99 \
					-D_GNU_SOURCE \
					-pthread -fvisibility=hidden
ifeq ($(ENABLE_SANITIZE),YES)
	LD=clang
	SANITIZE_LDLAGS=-fsanitize=address
endif
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup \
					$(SANITIZE_LDLAGS) \
					-keep_private_externs
endif

# OS X 11.x doesn't have /usr/lib/libSystem.dylib and needs an explicit setting.
ifeq ($(uname_S),Darwin)
ifeq ("$(wildcard /usr/lib/libSystem.dylib)","")
APPLE_LIBS = -L /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib -lsystem
endif
endif

.SUFFIXES: .c .so .xo .o
SOURCEDIR=$(shell pwd -P)
CC_SOURCES = $(wildcard $(SOURCEDIR)/*.c) \
	$(wildcard $(THREADPOOL_DIR)/thpool.c) \
	$(wildcard $(DEP_DIR)/*.c) 
CC_OBJECTS = $(sort $(patsubst %.c, %.o, $(CC_SOURCES)))


all: init ${THREADPOOL_DIR}/thpool.o redisxslot.so ldd_so

help:
	@echo "please choose make with below env params"
	@echo "BUILD_TYPE={Debug or Release} default Debug"
	@echo "ENABLE_SANITIZE={YES or NO} default NO"
	@echo "RM_INCLUDE_DIR={redis_absolute_path}/src, include redismodule.h"
	@echo "HIREDIS_USE_DYLIB=1, linker with use hiredis.so"
	@echo "HIREDIS_USE_DYLIB=1 HIREDIS_RUNTIME_DIR=/usr/local/lib ,if pkg install hiredis, linker with HIREDIS_RUNTIME_DIR use hiredis.so"
	@echo "REDIS_VERSION=6000, default 6000(6.0.0), use 70200(7.2.0) inlcude 7.2.0+ redismodule.h to use feature api"
	@echo "make docker_img to build latest redis-server load redisxslot module img"
	@echo "make docker_img_run to run latest redisxslot module docker img container"
	@echo "have fun :)"

init:
	@git submodule init
	@git submodule update
	@make -C $(SDK_DIR)/rmutil CFLAGS="-g -fPIC $(OPTIMIZE_CFLAGS) -std=gnu99 -Wall -Wno-unused-function -fvisibility=hidden -I$(RM_INCLUDE_DIR)"
	@make -C $(HIREDIS_DIR) OPTIMIZATION="$(OPTIMIZE_CFLAGS)" CFLAGS="-fvisibility=hidden" LDFLAGS="-fvisibility=hidden"
ifeq ($(HIREDIS_USE_DYLIB),1)
	@rm -rvf $(HIREDIS_RUNTIME_DIR)/libhiredis.so.1.1.0
	@ln -s $(HIREDIS_DIR)/libhiredis.so $(HIREDIS_RUNTIME_DIR)/libhiredis.so.1.1.0
endif

${THREADPOOL_DIR}/thpool.o: ${THREADPOOL_DIR}/thpool.c
	$(CC) -c -o $@ $(SHOBJ_CFLAGS) \
	$<

${SOURCEDIR}/module.o: ${SOURCEDIR}/module.c
	$(CC) -c -o $@ $(SHOBJ_CFLAGS) $(DEP_CFLAGS) \
	$(THREADPOOL_CFLAGS) \
	$(HIREDIS_CFLAGS) \
	$< 

${SOURCEDIR}/redisxslot.o: ${SOURCEDIR}/redisxslot.c
	$(CC) -c -o $@ $(SHOBJ_CFLAGS) $(DEP_CFLAGS) \
	$(THREADPOOL_CFLAGS) \
	$(HIREDIS_CFLAGS) \
	$<

%.o: %.c
	$(CC) -c -o $@ $(SHOBJ_CFLAGS) $<

redisxslot.so: $(CC_OBJECTS)
	$(LD) -o $@ $(CC_OBJECTS) \
	$(SHOBJ_LDFLAGS) \
	$(HIREDIS_LIB_FLAGS) \
	$(APPLE_LIBS) \
	-lc

ldd_so:
ifeq ($(uname_S),Darwin)
	@rm -rvf $(SOURCEDIR)/redisxslot.dylib.$(REDISXSLOT_SONAME)
	@otool -L $(SOURCEDIR)/redisxslot.so
	@ln -s $(SOURCEDIR)/redisxslot.so $(SOURCEDIR)/redisxslot.dylib.$(REDISXSLOT_SONAME)
else
	@rm -rvf $(SOURCEDIR)/redisxslot.so.$(REDISXSLOT_SONAME)
	@ldd $(SOURCEDIR)/redisxslot.so
	@ln -s $(SOURCEDIR)/redisxslot.so $(SOURCEDIR)/redisxslot.so.$(REDISXSLOT_SONAME)
endif

clean:
	cd $(SOURCEDIR) && rm -rvf *.xo *.so *.o *.a
	cd $(SOURCEDIR)/dep && rm -rvf *.xo *.so *.o *.a
	cd $(THREADPOOL_DIR) && rm -rvf *.xo *.so *.o *.a
	cd $(HIREDIS_DIR) && make clean 
	cd $(SDK_DIR)/rmutil && make clean 
	rm -rvf $(HIREDIS_RUNTIME_DIR)/libhiredis.so.1.1.0
	rm -rvf $(SOURCEDIR)/redisxslot.so.$(REDISXSLOT_SONAME)
	rm -rvf $(SOURCEDIR)/redisxslot.dylib.$(REDISXSLOT_SONAME)

# build docker img with redis stable version https://hub.docker.com/_/redis/ 
# (v6.0)6.0.20 (v6.2)6.2.13 (v7.0)7.0.12 (v7.2)7.2.0
docker_img:
	docker build -t redisxslot:latest_$(REDISXSLOT_SONAME) . --build-arg A_REDIS_IMG_TAG=latest --build-arg A_REDIS_SERVER_PORT=17000
docker_img_v6.0:
	docker build -t redisxslot:6.0.20_$(REDISXSLOT_SONAME) . --build-arg A_REDIS_IMG_TAG=6.0.20 --build-arg A_REDIS_SERVER_PORT=16001
docker_img_v6.2:
	docker build -t redisxslot:6.2.13_$(REDISXSLOT_SONAME) . --build-arg A_REDIS_IMG_TAG=6.2.13 --build-arg A_REDIS_SERVER_PORT=16002
docker_img_v7.0:
	docker build -t redisxslot:7.0.12_$(REDISXSLOT_SONAME) . --build-arg A_REDIS_IMG_TAG=7.0.12 --build-arg A_REDIS_SERVER_PORT=16003
docker_img_v7.2:
	docker build -t redisxslot:7.2.0_$(REDISXSLOT_SONAME) . --build-arg A_REDIS_IMG_TAG=7.2.0 --build-arg A_REDIS_SERVER_PORT=16004

# create bridge docker network
docker_network:
	docker network create -d bridge redisxslot-net

docker_img_list:
	docker image list | grep redisxslot

# run docker reidisxslot container, (taolu)so easy~
# just through bridge docker network for container inner run redis-cli 
# tips: 
# if container not a pod or vpc network, 
# need config inner container access outside network.
docker_img_run:
	docker run -itd \
	--name redisxslot \
	--network redisxslot-net \
	-p 17100:17000 \
	redisxslot:latest_$(REDISXSLOT_SONAME)
docker_img_run1:
	docker run -itd \
	--name redisxslot1 \
	--network redisxslot-net \
	-p 17101:17000 \
	redisxslot:latest_$(REDISXSLOT_SONAME)
docker_img_run_v6.0:
	docker run -itd \
	--name redisxslot_6.0.20_$(REDISXSLOT_SONAME) \
	--network redisxslot-net \
	-p 16100:16001 \
	redisxslot:6.0.20_$(REDISXSLOT_SONAME)
docker_img_run1_v6.0:
	docker run -itd \
	--name redisxslot1_6.0.20_$(REDISXSLOT_SONAME) \
	--network redisxslot-net \
	-p 16101:16001 \
	redisxslot:6.0.20_$(REDISXSLOT_SONAME)
docker_img_run_v6.2:
	docker run -itd \
	--name redisxslot_6.2.13_$(REDISXSLOT_SONAME) \
	--network redisxslot-net \
	-p 16200:16002 \
	redisxslot:6.2.13_$(REDISXSLOT_SONAME)
docker_img_run1_v6.2:
	docker run -itd \
	--name redisxslot1_6.2.13_$(REDISXSLOT_SONAME) \
	--network redisxslot-net \
	-p 16201:16002 \
	redisxslot:6.2.13_$(REDISXSLOT_SONAME)
docker_img_run_v7.0:
	docker run -itd \
	--name redisxslot_7.0.12_$(REDISXSLOT_SONAME) \
	--network redisxslot-net \
	-p 16300:16003 \
	redisxslot:7.0.12_$(REDISXSLOT_SONAME)
docker_img_run1_v7.0:
	docker run -itd \
	--name redisxslot1_7.0.12_$(REDISXSLOT_SONAME) \
	--network redisxslot-net \
	-p 16301:16003 \
	redisxslot:7.0.12_$(REDISXSLOT_SONAME)
docker_img_run_v7.2:
	docker run -itd \
	--name redisxslot_7.2.0_$(REDISXSLOT_SONAME)\
	--network redisxslot-net \
	-p 16400:16004 \
	redisxslot:7.2.0_$(REDISXSLOT_SONAME)
docker_img_run1_v7.2:
	docker run -itd \
	--name redisxslot1_7.2.0_$(REDISXSLOT_SONAME)\
	--network redisxslot-net \
	-p 16401:16004 \
	redisxslot:7.2.0_$(REDISXSLOT_SONAME)
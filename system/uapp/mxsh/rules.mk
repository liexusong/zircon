# Copyright 2016 The Fuchsia Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_DIR := $(GET_LOCAL_DIR)

MODULE := $(LOCAL_DIR)

MODULE_TYPE := userapp

MODULE_SRCS += \
    $(LOCAL_DIR)/mxsh.c \
    $(LOCAL_DIR)/builtin.c \

MODULE_NAME := mxsh

MODULE_DEPS := \
    ulib/mxio ulib/unittest ulib/musl ulib/ddk ulib/magenta

USER_MANIFEST_LINES += docs/LICENSE=kernel/LICENSE

include make/module.mk

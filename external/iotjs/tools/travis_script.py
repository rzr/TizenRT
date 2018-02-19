#!/usr/bin/env python

# Copyright 2017-present Samsung Electronics Co., Ltd. and other contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os

from common_py.system.filesystem import FileSystem as fs
from common_py.system.executor import Executor as ex
from common_py.system.platform import Platform
from check_tidy import check_tidy

platform = Platform()

DOCKER_ROOT_PATH = fs.join('/root')

# IoT.js path in travis
TRAVIS_BUILD_PATH = fs.join(os.environ['TRAVIS_BUILD_DIR'])

# IoT.js path in docker
DOCKER_IOTJS_PATH = fs.join(DOCKER_ROOT_PATH, 'work_space/iotjs')

DOCKER_TIZENRT_PATH = fs.join(DOCKER_ROOT_PATH, 'TizenRT')
DOCKER_TIZENRT_OS_PATH = fs.join(DOCKER_TIZENRT_PATH, 'os')
DOCKER_TIZENRT_OS_TOOLS_PATH = fs.join(DOCKER_TIZENRT_OS_PATH, 'tools')

DOCKER_NUTTX_PATH =fs.join(DOCKER_ROOT_PATH, 'nuttx')

DOCKER_NAME = 'iotjs_docker'
BUILDTYPES = ['debug', 'release']
TIZENRT_TAG = '1.1_Public_Release'

# Common buildoptions for sanitizer jobs.
BUILDOPTIONS_SANITIZER = [
    '--buildtype=debug',
    '--clean',
    '--compile-flag=-fno-common',
    '--compile-flag=-fno-omit-frame-pointer',
    '--jerry-cmake-param=-DFEATURE_SYSTEM_ALLOCATOR=ON',
    '--jerry-cmake-param=-DJERRY_LIBC=OFF',
    '--no-check-valgrind',
    '--no-snapshot',
    '--profile=test/profiles/host-linux.profile',
    '--run-test=full',
    '--target-arch=i686'
]

def run_docker():
    ex.check_run_cmd('docker', ['run', '-dit', '--privileged',
                     '--name', DOCKER_NAME, '-v',
                     '%s:%s' % (TRAVIS_BUILD_PATH, DOCKER_IOTJS_PATH),
                     'iotjs/ubuntu:0.4'])

def exec_docker(cwd, cmd):
    exec_cmd = 'cd %s && ' % cwd + ' '.join(cmd)
    ex.check_run_cmd('docker', [
                     'exec', '-it', DOCKER_NAME, 'bash', '-c', exec_cmd])

def set_release_config_tizenrt():
    exec_docker(DOCKER_ROOT_PATH, [
                'cp', 'tizenrt_release_config',
                fs.join(DOCKER_TIZENRT_OS_PATH, '.config')])

def build_iotjs(buildtype, args=[]):
    exec_docker(DOCKER_IOTJS_PATH, [
                './tools/build.py',
                '--clean',
                '--buildtype=' + buildtype] + args)

if __name__ == '__main__':
    if os.getenv('RUN_DOCKER') == 'yes':
        run_docker()

    test = os.getenv('OPTS')
    if test == 'host-linux':
        build_iotjs('debug', [
                    '--profile=profiles/minimal.profile'])

        for buildtype in BUILDTYPES:
            build_iotjs(buildtype, [
                        '--run-test=full',
                        '--profile=test/profiles/host-linux.profile'])

    elif test == 'rpi2':
        for buildtype in BUILDTYPES:
            build_iotjs(buildtype, [
                        '--target-arch=arm',
                        '--target-board=rpi2',
                        '--profile=test/profiles/rpi2-linux.profile'])

    elif test == 'artik053':
        # Checkout specified tag
        exec_docker(DOCKER_TIZENRT_PATH, ['git', 'checkout', TIZENRT_TAG])
        # Set configure
        exec_docker(DOCKER_TIZENRT_OS_TOOLS_PATH, [
                    './configure.sh', 'artik053/iotjs'])

        for buildtype in BUILDTYPES:
            if buildtype == 'release':
                set_release_config_tizenrt()
            exec_docker(DOCKER_TIZENRT_OS_PATH, [
                        'make', 'IOTJS_ROOT_DIR=' + DOCKER_IOTJS_PATH,
                        'IOTJS_BUILD_OPTION='
                        '--profile=test/profiles/tizenrt.profile'])

    elif test == 'stm32f4dis':
        for buildtype in BUILDTYPES:
            exec_docker(DOCKER_NUTTX_PATH, ['make', 'clean'])
            exec_docker(DOCKER_NUTTX_PATH, ['make', 'context'])
            # Build IoT.js
            build_iotjs(buildtype, [
                        '--target-arch=arm',
                        '--target-os=nuttx',
                        '--nuttx-home=' + DOCKER_NUTTX_PATH,
                        '--target-board=stm32f4dis',
                        '--jerry-heaplimit=78',
                        '--profile=test/profiles/nuttx.profile'])
            # Build Nuttx
            if buildtype == "release":
                rflag = 'R=1'
            else:
                rflag = 'R=0'
            exec_docker(DOCKER_NUTTX_PATH, [
                        'make', 'all',
                        'IOTJS_ROOT_DIR=' + DOCKER_IOTJS_PATH, rflag])

    elif test == 'tizen':
        exec_docker(DOCKER_IOTJS_PATH, ['config/tizen/gbsbuild.sh', '-y'])
        # TODO: Add release build test

    elif test == "misc":
        ex.check_run_cmd('tools/check_signed_off.sh', ['--travis'])

        if not check_tidy(TRAVIS_BUILD_PATH):
            ex.fail("Failed tidy check")

    elif test == "external-modules":
        for buildtype in BUILDTYPES:
            build_iotjs(buildtype, [
                        '--run-test=full',
                        '--profile=test/profiles/host-linux.profile',
                        '--external-modules=test/external_modules/'
                        'mymodule1,test/external_modules/mymodule2',
                        '--cmake-param=-DENABLE_MODULE_MYMODULE1=ON',
                        '--cmake-param=-DENABLE_MODULE_MYMODULE2=ON'])
            binary = fs.join('build',
                             platform.arch() + '-' + platform.os(),
                             buildtype, 'bin', 'iotjs')
            ext_mod_tests = [
                'test/external_modules/test_external_module1.js',
                'test/external_modules/test_external_module2.js']
            # TODO: Use testrunner to run an extended test set
            for test in ext_mod_tests:
                exec_docker(DOCKER_IOTJS_PATH, [binary, test])

    elif test == "no-snapshot":
        for buildtype in BUILDTYPES:
            build_iotjs(buildtype, ['--run-test=full', '--no-snapshot',
                                    '--jerry-lto'])

    elif test == "host-darwin":
        for buildtype in BUILDTYPES:
            ex.check_run_cmd('./tools/build.py', [
                             '--run-test=full',
                             '--buildtype=' + buildtype,
                             '--clean',
                             '--profile=test/profiles/host-darwin.profile'])

    elif test == "asan":
        ex.check_run_cmd('./tools/build.py', [
                         '--compile-flag=-fsanitize=address',
                         '--compile-flag=-O2'
                         ] + BUILDOPTIONS_SANITIZER)

    elif test == "ubsan":
        ex.check_run_cmd('./tools/build.py', [
                         '--compile-flag=-fsanitize=undefined'
                         ] + BUILDOPTIONS_SANITIZER)

    elif test == "coverity":
        ex.check_run_cmd('./tools/build.py', ['--clean'])

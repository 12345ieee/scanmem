"""
    GameConquerorBackend: communication with scanmem
    
    Copyright (C) 2010,2011,2013 Wang Lu <coolwanglu(a)gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

import sys
import os
import ctypes
import threading

import misc

class GameConquerorBackend():
    BACKEND_FUNCS = {
        'sm_init' : (ctypes.c_bool, ),
        'sm_set_backend' : (None, ),
        'sm_backend_exec_cmd' : (None, ctypes.c_char_p),
        'sm_get_num_matches' : (ctypes.c_long, ),
        'sm_get_version' : (ctypes.c_char_p, ),
        'sm_get_scan_progress' : (ctypes.c_double, ),
        'sm_set_stop_flag' : (ctypes.c_bool, )
    }

    def __init__(self, libpath='libscanmem.so'):
        self.lib = ctypes.CDLL(libpath)
        self.init_lib_functions()
        self.lib.sm_set_backend()
        self.lib.sm_init()
        self.send_command('reset')
        self.version = ''

    def init_lib_functions(self):
        for k,v in GameConquerorBackend.BACKEND_FUNCS.items():
            f = getattr(self.lib, k)
            f.restype = v[0]
            f.argtypes = v[1:]

    def exec_and_kill(self, cmd):
        self.lib.backend_exec_cmd(ctypes.c_char_p(misc.encode(cmd)))
        print 'a'
        os.close(sys.stdout.fileno())

    # `get_output` will return in a string what libscanmem would print to stdout
    def send_command(self, cmd, get_output=False):
        if get_output:
            libthread = threading.Thread(target=self.lib.sm_backend_exec_cmd,
                                         args=(cmd,))

            pipe_r, pipe_w = os.pipe()
            s = ''
            backup_stdout_filedesc = os.dup(sys.stdout.fileno())
            sys.stderr.write('bef start\n')

            os.dup2(pipe_w, sys.stdout.fileno())
            libthread.start()
            sys.stderr.write('started\n')
            while 1:
                chunk = os.read(pipe_r, 1)
                sys.stderr.write('ch: *'+chunk+'*\n')
                if chunk=='': break
                s += chunk
            sys.stderr.write('s built\n')
            libthread.join()
            sys.stderr.write('joined\n')
            os.dup2(backup_stdout_fileno, sys.stdout.fileno())

            sys.stderr.write('after read\n')
            os.close(backup_stdout_fileno)
            
            return s

        else:
            self.lib.sm_backend_exec_cmd(ctypes.c_char_p(misc.encode(cmd)))

    def get_match_count(self):
        return self.lib.sm_get_num_matches()

    def get_version(self):
        return misc.decode(self.lib.sm_get_version())

    def get_scan_progress(self):
        return self.lib.sm_get_scan_progress()

    def set_stop_flag(self, stop_flag):
        self.lib.sm_set_stop_flag(stop_flag)

# Hey, Emacs! This is a -*- Python -*- file!
#
# Copyright (C) 2006-2012 Opersys inc.
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

import platform, sys, re, os
from SCons.Environment import Environment, Base
from SCons.Builder import Builder
from SCons.Tool.textfile import _text_builder

# Get the cpu architechture we run on.
def get_arch():
    ARCH = platform.machine();
    
    # This happens on MinGW.
    if not ARCH and sys.platform == "win32": ARCH = "i386"

    if ARCH == 'i386' or \
       ARCH == 'i486' or \
       ARCH == 'i586' or \
       ARCH == 'i686' or \
       ARCH == 'x86' or \
       ARCH == 'x86_64':

	ARCH = 'x86'

    elif ARCH == 'Power Macintosh' or \
	 ARCH == 'ppc':

	ARCH = 'ppc'

    else:
	raise Exception("Cannot determine CPU type (%s) of this machine.\n" % ARCH)

    return ARCH

# Get the platform (os) name of the system.
def get_platform():
    PLATFORM = platform.system()

    if PLATFORM.startswith('Linux'):
	PLATFORM = 'linux'

    elif PLATFORM.startswith('Darwin'):
	PLATFORM = 'mac_os_x'

    elif PLATFORM.startswith('CYGWIN') or PLATFORM.startswith('Windows'):
	PLATFORM = 'windows'

    else:
	raise Exception("Cannot resolve system name (%s) of this machine.\n" % PLATFORM)

    return PLATFORM

# Get big/little endian
def get_endianness():
    if sys.byteorder == 'big':
	BUILD_ENDIAN = 'big'

    else:
	BUILD_ENDIAN = 'little'

    return BUILD_ENDIAN

def LinkPath(src, dst):
    dst = os.path.abspath(dst)
    src = os.path.abspath(src)
    nodes = os.path.split(dst)
    backtrack = ''

    while nodes[0] != '' or nodes[1] != '':
	if src.startswith(nodes[0]): 
	    break
	nodes = os.path.split(nodes[0])
	backtrack += '../'

    if nodes[0] == '/':
	return src

    return backtrack + src[len(nodes[0]+'/'):]

def Link(target, source, env):
    source = map(str, source)
    target = map(str, target)

    if len(source) != 1:
	return -1

    for t in target:

	if os.path.lexists(t):
	    os.unlink(t)

	os.symlink(LinkPath(source[0], t), t)

    return 0
    
def extract_serializable(target, source, env):
    """Extract All serializable_ops declarations from the sources and generate a serializable_array with it."""
    target = map(str,target)
    source = map(str,source)
    files = ""
    for f in source:
	files += " " + f

    serializable = map(str.strip, os.popen("sed -nre s/'.*DECLARE_KSERIALIZABLE_OPS\(([A-Za-z_][A-Za-z0-9_]*)\).*/\\1_serializable_ops/p'" + files).readlines())
    f = file(target[0], "w")
    for s in serializable:
	f.write("extern struct kserializable_ops %s;\n" % s)

    f.write("\n")
    f.write("const struct kserializable_ops *%s[] = {\n" % os.path.splitext(os.path.basename(f.name))[0])
    
    for s in serializable:
	f.write("    &%s,\n" % s)

    f.write("    (void *)0\n")
    f.write("};\n")
    f.close()

    return None

def extract_tests(target, source, env):
    """Extract All test functions from the sources and generate a test_array with it."""
    target = map(str,target)
    source = map(str,source)
    files = ""
    for f in source:
	files += " " + f

    tests = map(str.strip, os.popen("sed -nre s/'.*UNIT_TEST\(([A-Za-z_][A-Za-z0-9_]*)\).*/__test_\\1/p'" + files).readlines())
    f = file(target[0], "w")

    f.write('#include "test.h"\n\n')

    for s in tests:
	f.write("unit_test_t %s;\n" % s)

    f.write("\n")
    f.write("const unit_test_t *unit_test_array[] = {\n")
    
    for s in tests:
	f.write("    &%s,\n" % s)

    f.write("    (void *)0\n")
    f.write("};\n")
    f.close()

    return None

# This is our environment. It sets default values for a bunch
# of usual vars like CCFLAGS and manages the common config like
# debug. Modifying the config flag in the environment will
# Automaticatilly update the build flags.
class KEnvironment(Base):
    def __init__(self,
		 **keyw):

	# Start with no listener
	listeners = keyw.get('LISTENERS', None)
	keyw['LISTENERS'] = {}

	# Set default values.
	keyw.setdefault('CCFLAGS', ['-W', '-Wall', '$CONF_DEBUG_CCFLAGS', '$CONF_MUDFLAP_CCFLAGS'])
	keyw.setdefault('LDFLAGS', ['-rdynamic', '$CONF_DEBUG_LDFLAGS', '$CONF_MUDFLAP_LDFLAGS', '$CONF_MPATROL_LDFLAGS'])
	keyw.setdefault('LIBS', [])
	keyw.setdefault('CPPPATH', [])
	keyw.setdefault('LIBPATH', [])
	keyw.setdefault('PLATFORM', get_platform())
	keyw.setdefault('ARCH', get_arch())
	keyw.setdefault('ENDIAN', get_endianness())
	keyw.setdefault('DEBUG_CCFLAGS', ['-g', '-O0'])
	keyw.setdefault('DEBUG_LDFLAGS', ['-g'])
	keyw.setdefault('NDEBUG_CCFLAGS', ['-O2', '-fno-strict-aliasing', '-DNDEBUG'])
	keyw.setdefault('MUDFLAP_CCFLAGS', ['-fmudflap'])
	keyw.setdefault('MUDFLAP_LDFLAGS', ['-lmudflap'])
	keyw.setdefault('MAPTROL_LDFLAGS', ['-lmpatrol', '-lbfd'])
	keyw.setdefault('VERSION', '0.0')
        
        tools = None
        if get_platform() == 'windows': tools = ["mingw"]
        keyw['tools'] = tools

	# Initialize the Environment with those values.
	Base.__init__(self, **keyw)

	self.Append(BUILDERS = {'Link' : Builder(action = Link),
			        'ExtractSerializable' : Builder(action = extract_serializable, suffix = '.c', src_suffix = '.c'),
			        'ExtractTests' : Builder(action = extract_tests, suffix = '.c', src_suffix = '.c'),
                                'Textfile': _text_builder})

	# Set the listener and tell them the value has changed.
	if listeners:
	    self['LISTENERS'] = listeners
	else:
	    self['LISTENERS'] = {'debug' : debug_listener,
				 'PLATFORM' : platform_listener,
				 'mudflap' : mudflap_listener,
				 'mpatrol' : mpatrol_listener,
				 'VERSION' : version_listener }

	for key in self['LISTENERS'].keys():
	    self.dict_change(key)


    # Trac calls that modify our environment.
    def __setitem__(self, key, value):
	Base.__setitem__(self, key, value)
	self.dict_change(key)

    def __delitem__(self, key):
	Base.__delitem__(self, key)
	self.dict_change(key)

    # Call listener for environment changes.
    def dict_change(self, key):
	try:
	    self['LISTENERS'][key](self, key)
	except KeyError:
	    pass


### Listeners, set them as kenv['LISTENERS'][<env variable>] to be triggered on
### <env variable> changes. Exemple:
### env = KEnvironment(LISTENERS = {'debug' : debug_listener})

# Update debug flags, called when modifying self['debug']
def debug_listener(env, key):
    try:
	if env[key]:
	    env['CONF_DEBUG_CCFLAGS'] = ['$DEBUG_CCFLAGS']
	    env['CONF_DEBUG_LDFLAGS'] = ['$DEBUG_LDFLAGS']
	else:
	    env['CONF_DEBUG_CCFLAGS'] = ['$NDEBUG_CCFLAGS']
	    env['CONF_DEBUG_LDFLAGS'] = ['$NDEBUG_LDFLAGS']
    except KeyError:
	try:
	    del env['CONF_DEBUG_CCFLAGS']
	except KeyError:
	    pass

# Update the platform flag, called when modifying env['PLATFORM']
def platform_listener(env, key):
    try:
	if env['PLATFORM'] == 'windows':
	    env.Append(CPPDEFINES = ['__WINDOWS__']);
	else:
	    env.Append(CPPDEFINES = ['__UNIX__']);
    except KeyError:
	raise Exception("You should not undefined the platform")

# Update flags for mudflap, called when modifying env['mudflap']
def mudflap_listener(env, key):
    try:
	if env[key]:
	    env['CONF_MUDFLAP_CCFLAGS'] = '$MUDFLAP_CCFLAGS'
	    env['CONF_MUDFLAP_LDFLAGS'] = '$MUDFLAP_LDFLAGS'
	else:
	    del env['CONF_MUDFLAP_CCFLAGS']
	    del env['CONF_MUDFLAP_LDFLAGS']
    except KeyError:
	try:
	    del env['CONF_MUDFLAP_CCFLAGS']
	    del env['CONF_MUDFLAP_LDFLAGS']
	except KeyError:
	    pass

# Update flags for mpatrol, called when modifying env['mpatrol']
def mpatrol_listener(env, key):
    try:
	if env[key]:
	    env['CONF_MPATROL_LDFLAGS'] = '$MPATROL_LDFLAGS'
	else:
	    del env['CONF_MPATROL_LDFLAGS']
    except KeyError:
	try:
	    del env['CONF_MPATROL_LDFLAGS']
	except KeyError:
	    pass

def version_listener(env, key):
    try:
	env['BASE_VERSION'] = env['VERSION'].split('.')[0]
    except KeyError:
	pass

Environment = KEnvironment

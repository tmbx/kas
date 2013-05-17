###########################################################################
### INITIALIZE ############################################################
###########################################################################

### Verify Python version.
EnsurePythonVersion(2,3)

### Import modules.
import commands, sys, os, platform, re
from subprocess import *
from kenv import KEnvironment

### No target to build by default.
Default(None)


###########################################################################
### FUNCTIONS #############################################################
###########################################################################

def get_git_rev():
        proc = Popen(args = "git log -n 1 --pretty=oneline | cut -d ' ' -f 1", stdout=PIPE, shell = True)
        (git_rev, err) = proc.communicate()
        return git_rev.strip();

### This function prints the current configuration.
def print_current_config():
	sys.stdout.write("\nArgument values (modify using 'scons config arg=value'):\n%s\n" % opts_help)
	Exit(0)

### This function creates a list of static objects.
### Arguments:
### Build environment.
### Compiled objects destination directory.
### Objects source directory.
### Objects source list.
def get_static_object_list(env, build_dir, src_dir, src_list):

	regex = re.compile('(.+)\.\w+$')
	object_list = []
	
	for source in src_list:
		object_list.append(env.StaticObject(build_dir + regex.match(source).group(1), src_dir + source))
	
	return object_list

### This function creates a list of shared objects.
### Arguments:
### Build environment.
### Compiled objects destination directory.
### Objects source directory.
### Objects source list.
def get_shared_object_list(env, build_dir, src_dir, src_list):

	regex = re.compile('(.+)\.\w+$')
	object_list = []
	
	for source in src_list:
		object_list.append(env.SharedObject(build_dir + regex.match(source).group(1), src_dir + source))
	
	return object_list
    
### This function returns the target to build the kcd program.
def get_kcd_target():

    	src_list = 	[
	    	    	'kcd/frontend.c',
			'kcd/k3p.c',
			'kcd/kcd_misc.c',
			'kcd/kfs.c',
			'kcd/kmod_transfer.c',
			'kcd/kws.c',
                        'kcd/mail.c',
	    	    	'kcd/main.c',
			'kcd/mgt.c',
                        'kcd/misc_cmd.c',
                        'kcd/notif.c',
                        'kcd/ticket.c',
			'kcd/vnc.c',
			'common/anp.c',
			'common/anp_tls.c',
			'common/iniparser.c',
			'common/kdaemon.c',
                        'common/pg_common.c',
			'common/kmod_base.c',
			'common/ktls.c',
			'common/misc.c',
			'common/pg.c',
			'common/proxy.c',
			]
	
	cpp_path = 	[KTOOLS_CPP_PATH, 'common/', 'kcd']
	cpp_defines =	[]
	link_flags = 	['-rdynamic']
	lib_path =	[KTOOLS_LIB_PATH]
	lib_list = 	['ktools', 'gnutls', 'pq', 'mhash']
	
	git_rev = get_git_rev()
        if BUILD_ENV["PLATFORM"] == "windows":
		cpp_defines.append('-DBUILD_ID=\\"%s\\"' % git_rev);
	else:
		cpp_defines.append("-DBUILD_ID='\"%s\"'" % git_rev);
	
	env = BUILD_ENV.Clone()
	env.Append	(
			CPPPATH = cpp_path,
			CPPDEFINES = cpp_defines,
			CCFLAGS = [],
			LINKFLAGS = link_flags,
			LIBPATH = lib_path,
			LIBS = lib_list,
			)
	
	kcd_target = 'build/kcd'
	
	return env.Program(
		target = kcd_target,
		source = get_static_object_list(env, 'build/kcd', '', src_list),
		)

### This function returns the target to build the kcdpg shared library.
def get_kcdpg_target():
    	src_list = 	[
                        'kcdpg/lib.c',
			'common/anp.c',
                        'common/pg_common.c',
			'common/kmod_base.c'
			]
	
        proc = Popen(args = ["pg_config", "--includedir-server", "--includedir"], stdout=PIPE, shell = False)
        (pg_hdr, err) = proc.communicate()
        
	cpp_path = 	[KTOOLS_CPP_PATH, 'common', 'kcdpg', pg_hdr.splitlines()]
	cpp_defines =	[]
	link_flags = 	[]
	lib_path =	[KTOOLS_LIB_PATH]
	lib_list = 	['ktools']
	
	env = BUILD_ENV.Clone()
	env.Append	(
			CPPPATH = cpp_path,
			CPPDEFINES = cpp_defines,
			CCFLAGS = [],
			LINKFLAGS = link_flags,
			LIBPATH = lib_path,
			LIBS = lib_list,
			)
	
	kcd_target = 'build/kcdpg'
	
	return env.SharedLibrary(
		target = kcd_target,
		source = get_shared_object_list(env, 'build/kcdpg', '', src_list),
		)
    

### This function returns the target to build the ktlstunnel program.
def get_ktlstunnel_target():

    	src_list = 	[
	    	    	'ktlstunnel/main.c',
			'ktlstunnel/tunnel.c',
			'common/kmod_base.c',
			'common/ktls.c',
			'common/misc.c',
			'common/proxy.c',
			]
	
        cpp_path = [KTOOLS_CPP_PATH, 'common/', 'ktlstunnel']
        
        if BUILD_ENV["PLATFORM"] == "windows":
                cpp_path += [ 'C:/birtz/lib/pthreads-w32-2-8-0-release/teambox',
                              'C:/birtz/lib/gnutls-2.4.1/teambox'
                            ]
	cpp_defines =	[]
	link_flags = 	[]
	lib_path =	[KTOOLS_LIB_PATH]
        

        lib_list = 	['ktools', 'gnutls']
        
        if BUILD_ENV["PLATFORM"] == "windows":
                lib_list += [ 'ws2_32', 'pthread' ]
                lib_path += [ 'C:/birtz/lib/pthreads-w32-2-8-0-release/teambox',
                              'C:/birtz/lib/gnutls-2.4.1/teambox'
                            ]
         
	git_rev = get_git_rev()
        if BUILD_ENV["PLATFORM"] == "windows":
		cpp_defines.append('-DBUILD_ID=\\"%s\\"' % git_rev);
	else:
		cpp_defines.append("-DBUILD_ID='\"%s\"'" % git_rev);
	
	env = BUILD_ENV.Clone()
	env.Append	(
			CPPPATH = cpp_path,
			CPPDEFINES = cpp_defines,
			CCFLAGS = [],
			LINKFLAGS = link_flags,
			LIBPATH = lib_path,
			LIBS = lib_list,
			)
        
        
	ktlstunnel_target = 'build/ktlstunnel'
	
	return env.Program(
		target = ktlstunnel_target,
		source = get_static_object_list(env, 'build/ktlstunnel', '', src_list),
		)


### This function returns the target to build the kcd program.
def get_vnc_target():

    	src_list = 	[
			'vnc_reflector/main.c',
			'vnc_reflector/logging.c',
			'vnc_reflector/active.c',
			'vnc_reflector/actions.c',
			'vnc_reflector/host_connect.c',
			'vnc_reflector/d3des.c',
			'vnc_reflector/rfblib.c',
			'vnc_reflector/async_io.c',
			'vnc_reflector/host_io.c',
			'vnc_reflector/client_io.c',
			'vnc_reflector/encode.c',
			'vnc_reflector/region.c',
			'vnc_reflector/translate.c',
			'vnc_reflector/control.c',
			'vnc_reflector/encode_tight.c',
			'vnc_reflector/decode_hextile.c',
			'vnc_reflector/decode_tight.c',
			'vnc_reflector/fbs_files.c',
			'vnc_reflector/region_more.c',
                        'vnc_reflector/decode_cursor.c',
			]
	
	cpp_path = 	['vnc_reflector/']
	cpp_defines =	['USE_POLL']
	link_flags = 	[]
	lib_path =	[]
	lib_list = 	['jpeg', 'z']
	
	env = BUILD_ENV.Clone()
	env.Append	(
			CPPPATH = cpp_path,
			CPPDEFINES = cpp_defines,
			CCFLAGS = ['-w'],
			LINKFLAGS = link_flags,
			LIBPATH = lib_path,
			LIBS = lib_list,
			)
	
	vnc_target = 'build/vncreflector'
	
	return env.Program(
		target = vnc_target,
		source = get_static_object_list(env, 'build/vncreflector', '', src_list),
		)
    	
		
# This function populates the build list and returns it. It's OK to call this
# function many times, it will only populate the list once. If 'buid_flag' is
# true, build targets will be added. If 'install_flag' is true, installation
# targets will be added.
def get_build_list(build_flag, install_flag):
	
	global build_list
	global build_list_init
	
	if build_list_init:
		return build_list
	
	build_list_init = 1
	
	if KCD_FLAG:
	    t = get_kcd_target()
	    if build_flag: build_list.append(t)
	    if install_flag: build_list.append(AlwaysBuild(BUILD_ENV.Install(BINDIR, source=t)))
        
        if KCDPG_FLAG:
            t = get_kcdpg_target()
	    if build_flag: build_list.append(t)
	    if install_flag: build_list.append(AlwaysBuild(BUILD_ENV.Install(PGPKGLIBDIR, source=t)))
	
	if KTLSTUNNEL_FLAG:
	    t = get_ktlstunnel_target()
	    if build_flag: build_list.append(t)
	    if install_flag: build_list.append(AlwaysBuild(BUILD_ENV.Install(BINDIR, source=t)))
	    
	if VNC_FLAG:
	    t = get_vnc_target()
	    if build_flag: build_list.append(t)
	    if install_flag: build_list.append(AlwaysBuild(BUILD_ENV.Install(BINDIR, source=t)))
	
	return build_list


###########################################################################
### OPTIONS ###############################################################
###########################################################################

### Create options environment.
opts_env = Environment()

### Load the options values.
opts = Options('build/build.conf')
opts.AddOptions	(
		(BoolOption('debug', 'enable debugging', 1)),
		(BoolOption('kcd', 'build kcd', 1)),
		(BoolOption('kcdpg', 'build kcdpg', 1)),
		(BoolOption('ktlstunnel', 'build ktlstunnel', 1)),
		(BoolOption('vnc', 'build vncreflector', 1)),
		('libktools_include', 'Location of include files for libktools', '#../libktools/src'),
		('libktools_lib', 'Location of library files for libktools', '#../libktools/build'),
		("DESTDIR", 'Root of installation', '/'),
		('BINDIR', 'Executable path', '/bin'),
		('PGPKGLIBDIR', 'Postgresql library path', '/usr/lib/postgresql/9.1/lib'),
		)
		
opts.Update(opts_env)
opts_dict = opts_env.Dictionary()

### Get the configuration values.
DEBUG_FLAG = opts_dict['debug']
KCD_FLAG = opts_dict['kcd']
KCDPG_FLAG = opts_dict['kcdpg']
KTLSTUNNEL_FLAG = opts_dict['ktlstunnel']
VNC_FLAG = opts_dict['vnc']
KTOOLS_CPP_PATH = opts_dict['libktools_include']
KTOOLS_LIB_PATH = opts_dict['libktools_lib']
DESTDIR = os.path.normpath(opts_dict['DESTDIR'])
BINDIR = os.path.normpath(opts_dict['DESTDIR'] + "/" + opts_dict['BINDIR'])
PGPKGLIBDIR = os.path.normpath(opts_dict['DESTDIR'] + "/" + opts_dict['PGPKGLIBDIR'])

### Update the options values and save.
if not os.path.isdir('build/'):
    os.mkdir('build/')
    
opts.Save('build/build.conf', opts_env)

### Setup help text.
help_text = "Type: 'scons config [-Q]' to show current configuration.\n"\
	    "      'scons build' to build the targets.\n"\
	    "      'scons install' to install the targets.\n"\
	    "      'scons uninstall' to uninstall the targets.\n"\
	    "      'scons clean' to clean built targets.\n"
opts_help = opts.GenerateHelpText(opts_env)
Help(help_text)


###########################################################################
### BUILD TARGET HANDLING #################################################
###########################################################################

### Create build environment.
BUILD_ENV = KEnvironment(ENV = os.environ)

### Set compilation flags.
BUILD_ENV.Append(CCFLAGS = [ '-fno-strict-aliasing'])

### Get GCC version.
gcc_version = 4
try:
    gcc_version_match = re.compile('^(\d)\.').match(BUILD_ENV["CXXVERSION"])
    if gcc_version_match: gcc_version = int(gcc_version_match.group(1))
except: pass
    

if gcc_version >= 4:
	BUILD_ENV.Append(CCFLAGS = [ '-Wno-pointer-sign' ])

if DEBUG_FLAG:
	BUILD_ENV.Append(CCFLAGS = [ '-g' ])
else:
	BUILD_ENV.Append(CCFLAGS = [ '-O2' ])
	BUILD_ENV.Append(CPPDEFINES = ['NDEBUG'])

if BUILD_ENV['ENDIAN'] == 'big':
    	BUILD_ENV.Append(CPPDEFINES = ['__BIG_ENDIAN__'])
else:
    	BUILD_ENV.Append(CPPDEFINES = ['__LITTLE_ENDIAN__'])

### The list of targets to build/clean.
build_list = []

### True if the build_list has been initialized.
build_list_init = 0


###########################################################################
### PHONY TARGETS #########################################################
###########################################################################

### Config: allow user to review current configuration. Include instructions
### on how to change it.
if 'config' in COMMAND_LINE_TARGETS:
	Alias("config", None)
	print_current_config()
	
### Build: build targets according to configuration.
elif 'build' in COMMAND_LINE_TARGETS:
	Alias("build", get_build_list(1, 0))

### Install: install targets at the location specified.
elif 'install' in COMMAND_LINE_TARGETS:
	bl = get_build_list(0, 1)
	Alias("install", get_build_list(0, 1))

### Uninstall: uninstall targets at the location specified. Also destroy the
### build, sigh. We might want to fix this eventually by bypassing SCons.
elif 'uninstall' in COMMAND_LINE_TARGETS:
	SetOption("clean", 1)
	Alias("uninstall", get_build_list(0, 1))
	
### Clean: clean built targets.
elif 'clean' in COMMAND_LINE_TARGETS:
	SetOption("clean", 1)
	Alias("clean", get_build_list(1, 0))

### No targets specified.
elif len(COMMAND_LINE_TARGETS) == 0:
		
	### Just print help.
	sys.stdout.write("\n%s\n" % help_text)
	Exit(0)
	

import os

if hasattr(os,'uname'):
    system = os.uname()[0]
else:
    system = 'Windows'

# install location
AddOption('--prefix',
          dest='prefix',
          type='string',
          nargs=1,
          action='store',
          metavar='DIR',
          help='installation prefix')
# debug flags for compliation
debug = ARGUMENTS.get('debug',1)

if not GetOption('prefix')==None:
    install_prefix = GetOption('prefix')
else:
    install_prefix = '/usr/local/'

Help("""
Type: 'scons lib'  to build libdataio
      'scons test' to build test programs
      'scons install' to install libraries and headers under %s
      (use --prefix  to change library installation location)

Options:
      debug=0      to disable debug compliation
""" % install_prefix)

env = Environment(ENV = os.environ,
                  CCFLAGS=['-Wall'],
                  LIBS=['m'],
                  PREFIX=install_prefix,
                  tools=['default'])

if 'CC' in os.environ:
    env.Replace(CC=os.environ['CC'])

env.ParseConfig("pkg-config libjpeg --cflags --libs")
if system=='Darwin':
    env.Append(CPPPATH=['/opt/local/include'],
               LIBPATH=['/opt/local/lib'],
               LIBS=['hdf5'])
else:
    env.ParseConfig("pkg-config hdf5 --cflags --libs")
    env.Append(CCFLAGS=['-fPIC'])

if int(debug):
    env.Append(CCFLAGS=['-g2'])
else:
    env.Append(CCFLAGS=['-O2'])

headers = 'lblio.h pcmio.h toeio.h vidio.h'.split(' ')

lib = env.Library('dataio', [x for x in env.Glob('*.c') if not str(x).startswith('test')])
# slib = env.SharedLibrary('dataio', [x for x in env.Glob('*.c') if not str(x).startswith('test')])

test_env = env.Clone()
test = [test_env.Program(os.path.splitext(str(x))[0], [x, lib]) \
            for x in env.Glob('test*.c')]

env.Alias('lib',lib)
env.Alias('test',test)

env.Alias('install', env.Install(os.path.join(env['PREFIX'],'lib'), [lib]))
env.Alias('install', env.Install(os.path.join(env['PREFIX'],'include'), headers))

project('python', 'cpp', version : '0.1')

includes  = include_directories('..')
python    = dependency('python-3.11-embed')
boost_py  = dependency('boost', modules : ['python311'])

ewma_py = shared_library('ewma_py', 'ewma_py.cpp', dependencies : [python, boost_py], include_directories : includes)

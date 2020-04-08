import ast
import os

import numpy as np
from setuptools import setup, find_packages, Extension

if ast.literal_eval(os.environ.get('SBO_STRING_DEBUG_BUILD', '0')):
    optlevel = 0
    debug_symbols = True
else:
    optlevel = 3
    debug_symbols = False


def extension(*args, **kwargs):
    flags = ['-std=gnu++17', f'-O{optlevel}']
    if debug_symbols:
        flags.append('-g')
    return Extension(
        *args,
        extra_compile_args=flags,
        include_dirs=[np.get_include()],
        **kwargs
    )


setup(
    name='sbo_string_dtype',
    version='0.0.1',
    description='An efficient string representation for numpy.',
    author='Joe Jevnik',
    author_email='joejev@gmail.com',
    packages=find_packages(),
    long_description='',
    license='Apache 2.0',
    classifiers=[],
    url='https://github.com/llllllllll/sbo-string-dtype',
    ext_modules=[
        extension(
            'sbo_string_dtype._dtype',
            ['sbo_string_dtype/_dtype.cc'],
        ),
    ],
    install_requires=['numpy'],
)

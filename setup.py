from setuptools import setup, find_packages
from setuptools.extension import Extension

ext_modules = [Extension("heapstruct._heapstruct", ["heapstruct/heapstruct.c"])]

setup(
    name="heapstruct",
    version="0.0.1",
    maintainer="Vizonex",
    author="Vizonex",
    description="A C Dataclass Forked From Quickle",
    classifiers=[
        "License :: OSI Approved :: BSD License",
        "Programming Language :: Python :: 3.9",
    ],
    license="BSD-3",
    packages=find_packages(),
    ext_modules=ext_modules,
    python_requires=">=3.9",
    zip_safe=False,
)

import os
import sys
import platform
import subprocess

from setuptools import setup, find_packages, Extension
from setuptools.command.build_ext import build_ext
from distutils.version import LooseVersion

class CMakeExtension(Extension):
	def __init__(self, name, sourcedir=''):
		Extension.__init__(self, name, sources=[])
		self.sourcedir = os.path.abspath(sourcedir)

class builder_ext(build_ext):
	def run(self):
		for ext in self.extensions:
			self.build_extension(ext)

	def build_extension(self, ext):
		extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
		
		cfg = "Debug" if self.debug else "Release"

		cmake_args = [f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
					  f"-DCMAKE_BUILD_TYPE={cfg}",
					  f"-DPYTHON_EXECUTABLE={sys.executable}"]
		build_args = [ "--config", cfg ]

		if platform.system() == "Windows":
			cmake_args += [ f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{cfg.upper()}={extdir}" ]

			if sys.maxsize > 2**32:
				cmake_args += [ "-A", "x64" ]

		env = os.environ.copy()
#		env["CXXFLAGS"] = '{} -DVERSION_INFO=\\"{}\\"'.format(env.get('CXXFLAGS', ''),
#								self.distribution.get_version())

		if not os.path.exists(self.build_temp):
			os.makedirs(self.build_temp)

		subprocess.check_call([ "cmake", ext.sourcedir ] + cmake_args, cwd=self.build_temp, env=env)
		subprocess.check_call([ "cmake", "--build", "." ] + build_args, cwd=self.build_temp)

setup(
	name="CellModeller5",
	version="1.0",
	packages=find_packages(),
	install_requires=[ "numpy" ],
	include_package_data=True,
	ext_modules=[CMakeExtension("CellModeller5")],
	cmdclass={
		"build_ext": builder_ext,
	},
	zip_safe=False,
)
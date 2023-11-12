from conan import ConanFile
from conan.tools.files import chdir, copy, mkdir
from os.path import join
from conan.tools.gnu import PkgConfigDeps

class BasicConanfile(ConanFile):
    name = "msgstream"
    version = "0.2.1"
    description = "Write messages with boundaries to streams"
    license = "MIT"
    homepage = "https://gulachek.com"

    def source(self):
        self.run("git clone git@github.com:gulachek/msgstream.git")

    def requirements(self):
        self.test_requires('boost/1.83.0')

    def build_requirements(self):
        # TODO - node and npm install?
        pass

    def generate(self):
        pc = PkgConfigDeps(self)
        pc.generate()

    # This method is used to build the source code of the recipe using the desired commands.
    def build(self):
        with chdir(self, 'msgstream'):
            self.run("npm install")
            self.run("node make.js")

    def package(self):
        d = join(self.source_folder, 'msgstream')
        build = join(d, "build")
        include = join(d, "include")
        copy(self, "*.h", include, join(self.package_folder, "include"))
        copy(self, "libmsgstream.dylib", build, join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.libs = ["msgstream"]
